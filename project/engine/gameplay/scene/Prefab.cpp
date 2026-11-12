#include "Prefab.h"
#include "Actor.h"
#include "Logger.h"
#include "TypeRegistry.h"

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Tako {

  namespace {

    constexpr std::string_view kPrefabsDirectory = "resources/Json/Prefabs/";
    constexpr std::string_view kPrefabExtension  = ".prefab.json";

    using PrefabMap = std::map<std::string, nlohmann::json, std::less<>>;

    // ponytail: 読み直しはエディタで保存したときだけ。外でファイルを書き換えたら再起動で反映
    PrefabMap& NormalizedCache()
    {
      static PrefabMap cache;
      return cache;
    }

#ifdef _DEBUG
    /// <summary>
    /// SetPrefabInMemory で置いた、ファイルの代わりの中身
    /// </summary>
    PrefabMap& MemoryPrefabs()
    {
      static PrefabMap prefabs;
      return prefabs;
    }
#endif

    const nlohmann::json* FindNormalized(std::string_view name);

    /// <summary>
    /// 置いた物（プレハブとの差分だけの 1 件）か。全部の値の 1 件は type を持つ
    /// </summary>
    bool IsPlaced(const nlohmann::json& entry)
    {
      return !entry.contains("type") && entry.contains("prefab") && entry["prefab"].is_string();
    }

    void ErasePrefabLink(nlohmann::json& entry)
    {
      entry.erase("prefab");
      entry.erase("node");
      entry.erase("root");
    }

    auto FindEntry(const nlohmann::json& entries, ActorId id)
    {
      return std::ranges::find_if(entries, [id](const nlohmann::json& entry) { return entry.value("id", ActorId{ 0 }) == id; });
    }

    /// <summary>
    /// 中に置いたプレハブに後から足された Actor の番号。同じ (根, 内側の番号) からは、いつも同じ番号になる
    /// </summary>
    ActorId MakeImplicitNode(ActorId root, ActorId node)
    {
      // ponytail: FNV-1a の 63 ビット。違う組み合わせが同じ番号になる可能性は 0 ではない。起きたら、外側のプレハブを Apply し直して番号をファイルに書く
      uint64_t hash = 14695981039346656037ull;
      for (const uint64_t value : { root, node }) {
        for (int shift = 0; shift < 64; shift += 8) {
          hash ^= (value >> shift) & 0xffull;
          hash *= 1099511628211ull;
        }
      }
      return kImplicitPrefabNode | hash;
    }

    /// <summary>
    /// プレハブの各 Actor を一度 Actor に読んで書き直した配列。差分を取る基準にする（float の誤差や、後から足した TPROPERTY を差分にしないため）。
    /// 中に置いた別のプレハブは開いて並べ、そのつながり（prefab / node / root）を各件に残す
    /// </summary>
    nlohmann::json Normalize(std::string_view name, const nlohmann::json& raw)
    {
      nlohmann::json entries;
      if (auto actors = raw.find("actors"); actors != raw.end() && actors->is_array()) {
        entries = *actors;
      } else {
        // 古い形（Actor 1 つ）は、番号 1 の根だけの組として読む
        entries = nlohmann::json::array({ raw });
        entries[0]["id"] = ActorId{ 1 };
        entries[0].erase("parent");
      }
      entries = ExpandPlacedActors(std::move(entries), MakeImplicitNode);

      nlohmann::json normalized = nlohmann::json::array();
      for (const nlohmann::json& entry : entries) {
        std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(entry.value("type", std::string("Actor")));
        if (!object || !object->IsA<Actor>() || entry.value("id", ActorId{ 0 }) == 0) {
          Logger::Log("Prefab: an actor of \"%.*s\" has an unknown type or no id and was skipped\n", static_cast<int>(name.size()), name.data());
          continue;
        }
        Actor& actor = static_cast<Actor&>(*object);
        actor.Load(entry);
        nlohmann::json saved = actor.Save();
        for (const char* key : { "prefab", "node", "root" }) {
          if (auto link = entry.find(key); link != entry.end()) {
            saved[key] = *link;
          }
        }
        normalized.push_back(std::move(saved));
      }
      return normalized;
    }

    const nlohmann::json* FindNormalized(std::string_view name)
    {
      PrefabMap& cache = NormalizedCache();
      if (auto it = cache.find(name); it != cache.end()) {
        return &it->second;
      }
      if (name.empty()) {
        return nullptr;
      }
      // 開いている途中のプレハブが、中に自分を持っていた（A の中に A、A → B → A）
      static std::vector<std::string> resolving;
      if (std::ranges::any_of(resolving, [name](const std::string& item) { return item == name; })) {
        Logger::Log("Prefab: \"%.*s\" contains itself. The inner one was skipped\n", static_cast<int>(name.size()), name.data());
        return nullptr;
      }

      nlohmann::json raw;
#ifdef _DEBUG
      if (auto memory = MemoryPrefabs().find(name); memory != MemoryPrefabs().end()) {
        raw = memory->second;
      }
#endif
      if (raw.is_null()) {
        if (!std::filesystem::is_regular_file(GetPrefabPath(name))) {
          return nullptr;
        }
        try {
          std::ifstream file(GetPrefabPath(name));
          raw = nlohmann::json::parse(file);
        } catch (const nlohmann::json::exception& e) {
          Logger::Log("Prefab: failed to read %s: %s\n", GetPrefabPath(name).c_str(), e.what());
          return nullptr;
        }
      }
      resolving.emplace_back(name);
      nlohmann::json normalized = Normalize(name, raw);
      resolving.pop_back();
      return &cache.emplace(std::string(name), std::move(normalized)).first->second;
    }

    /// <summary>
    /// key の ID を表で付け替える。表に無ければ、残すか key ごと消す
    /// </summary>
    /// <returns>表にあったか、残したなら true</returns>
    bool RemapKey(nlohmann::json& entry, const char* key, const ActorIdMap& ids, bool keepsUnmapped)
    {
      auto it = entry.find(key);
      if (it == entry.end() || !it->is_number_unsigned()) {
        return true;
      }
      if (auto mapped = ids.find(it->get<ActorId>()); mapped != ids.end()) {
        *it = mapped->second;
        return true;
      }
      if (!keepsUnmapped) {
        entry.erase(it);
      }
      return keepsUnmapped;
    }

    /// <summary>
    /// 値の中の ActorRef（{ "actor": ID } の形）を付け替える。表に無い相手は、残すか 0 にする
    /// </summary>
    void RemapActorRefs(nlohmann::json& value, const ActorIdMap& ids, bool keepsUnmapped)
    {
      if (value.is_object() && value.size() == 1 && value.contains("actor") && value["actor"].is_number_unsigned()) {
        nlohmann::json& id = value["actor"];
        if (auto mapped = ids.find(id.get<ActorId>()); mapped != ids.end()) {
          id = mapped->second;
        } else if (!keepsUnmapped) {
          id = ActorId{ 0 };
        }
        return;
      }
      if (value.is_structured()) {
        for (nlohmann::json& child : value) {
          RemapActorRefs(child, ids, keepsUnmapped);
        }
      }
    }

    /// <summary>
    /// プレハブの中で、node か、その親のどれかが removed に入っているか
    /// </summary>
    bool IsRemoved(const nlohmann::json& prefab, ActorId node, const std::vector<ActorId>& removed)
    {
      // 親をたどる回数は Actor の数まで（親が輪になったファイルで回り続けない）
      for (size_t step = 0; node != 0 && step <= prefab.size(); ++step) {
        if (std::ranges::find(removed, node) != removed.end()) {
          return true;
        }
        const auto found = FindEntry(prefab, node);
        node = found != prefab.end() ? found->value("parent", ActorId{ 0 }) : 0;
      }
      return false;
    }

    /// <summary>
    /// outer が target か、中に（何段でも）target を置いているか
    /// </summary>
    bool ContainsPrefab(std::string_view outer, std::string_view target)
    {
      if (outer == target) {
        return true;
      }
      const nlohmann::json* prefab = FindNormalized(outer);
      return prefab && std::ranges::any_of(*prefab, [target](const nlohmann::json& entry) {
        return entry.contains("prefab") && !entry.contains("root") && ContainsPrefab(entry["prefab"].get<std::string>(), target);
      });
    }

  } // namespace

  std::string GetPrefabPath(std::string_view name)
  {
    return std::string(kPrefabsDirectory) + std::string(name) + std::string(kPrefabExtension);
  }

  bool HasPrefab(std::string_view name)
  {
    return FindNormalized(name) != nullptr;
  }

  std::vector<std::string> GetPrefabNames()
  {
    std::vector<std::string> names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(kPrefabsDirectory, error)) {
      const std::string file = entry.path().filename().string();
      if (entry.is_regular_file() && file.ends_with(kPrefabExtension)) {
        names.push_back(file.substr(0, file.size() - kPrefabExtension.size()));
      }
    }
    std::ranges::sort(names);
    return names;
  }

  std::vector<ActorId> GetPrefabNodes(std::string_view name)
  {
    std::vector<ActorId> nodes;
    if (const nlohmann::json* prefab = FindNormalized(name)) {
      for (const nlohmann::json& entry : *prefab) {
        nodes.push_back(entry.value("id", ActorId{ 0 }));
      }
    }
    return nodes;
  }

  PrefabLink GetNestedPrefabLink(std::string_view name, ActorId node)
  {
    if (const nlohmann::json* prefab = FindNormalized(name)) {
      if (const auto found = FindEntry(*prefab, node); found != prefab->end() && found->contains("prefab")) {
        return { (*found)["prefab"].get<std::string>(), found->value("node", ActorId{ 0 }), found->value("root", ActorId{ 0 }) };
      }
    }
    return {};
  }

  nlohmann::json SaveActorEntry(Actor& actor)
  {
    nlohmann::json entry = actor.Save();
    if (!actor.GetPrefab().empty()) {
      entry["prefab"] = actor.GetPrefab();
      entry["node"]   = actor.GetPrefabNode();
      if (!actor.IsPrefabRoot()) {
        entry["root"] = actor.GetPrefabRoot();
      }
    }
    return entry;
  }

  nlohmann::json SaveActorTree(Actor& actor)
  {
    nlohmann::json actors = nlohmann::json::array();
    const auto add = [&actors](const auto& self, Actor& current) -> void {
      actors.push_back(SaveActorEntry(current));
      for (Actor* child : current.GetChildren()) {
        self(self, *child);
      }
    };
    add(add, actor);
    return { { "actors", std::move(actors) } };
  }

  void RemapActorIds(nlohmann::json& json, const ActorIdMap& ids, bool keepsUnmapped)
  {
    if (json.is_array()) {
      for (nlohmann::json& entry : json) {
        RemapActorIds(entry, ids, keepsUnmapped);
      }
      return;
    }
    if (!json.is_object()) {
      return;
    }
    RemapKey(json, "id", ids, keepsUnmapped);
    RemapKey(json, "parent", ids, keepsUnmapped);
    if (!RemapKey(json, "root", ids, false)) {
      ErasePrefabLink(json);
    }
    for (const char* key : { "properties", "components" }) {
      if (auto it = json.find(key); it != json.end()) {
        RemapActorRefs(*it, ids, keepsUnmapped);
      }
    }
  }

  nlohmann::json ExpandPlacedActors(nlohmann::json entries, const std::function<ActorId(ActorId root, ActorId node)>& newId)
  {
    // 置いた物を、組ごとにまとめる
    struct Instance {
      std::string          prefab;
      ActorIdMap           ids;      ///< プレハブの中の番号 → ID
      std::vector<ActorId> removed;  ///< 組から消した番号
    };
    std::map<ActorId, Instance> instances;
    for (nlohmann::json& entry : entries) {
      if (!IsPlaced(entry)) {
        continue;
      }
      const std::string          prefab = entry["prefab"].get<std::string>();
      const std::vector<ActorId> nodes  = GetPrefabNodes(prefab);
      if (nodes.empty()) {
        continue;
      }
      // node の無い物は、Actor 1 つのプレハブだった頃の保存
      if (!entry.contains("node")) {
        entry["node"] = nodes.front();
      }
      const ActorId node = entry["node"].get<ActorId>();
      if (entry.value("id", ActorId{ 0 }) == 0) {
        entry["id"] = newId(entry.value("root", ActorId{ 0 }), node);
      }
      const ActorId id   = entry["id"].get<ActorId>();
      Instance&     item = instances[entry.value("root", id)];
      item.prefab    = prefab;
      item.ids[node] = id;
      if (auto removed = entry.find("removed"); removed != entry.end() && removed->is_array() && !entry.contains("root")) {
        for (const nlohmann::json& number : *removed) {
          if (number.is_number_unsigned()) { item.removed.push_back(number.get<ActorId>()); }
        }
      }
    }
    // プレハブに後から足された Actor を、置いてある組にも足す（根が残っている組だけ。消した物と、その子孫は足さない）
    for (auto& [root, item] : instances) {
      const nlohmann::json* prefab = FindNormalized(item.prefab);
      if (!prefab || prefab->empty() || !item.ids.contains(prefab->front().value("id", ActorId{ 0 }))) {
        continue;
      }
      for (const nlohmann::json& source : *prefab) {
        const ActorId node = source.value("id", ActorId{ 0 });
        if (item.ids.contains(node) || IsRemoved(*prefab, node, item.removed)) {
          continue;
        }
        const ActorId id = newId(root, node);
        item.ids[node] = id;
        entries.push_back({ { "id", id }, { "prefab", item.prefab }, { "node", node }, { "root", root } });
      }
    }

    nlohmann::json expanded = nlohmann::json::array();
    for (const nlohmann::json& entry : entries) {
      if (!IsPlaced(entry)) {
        expanded.push_back(entry);
        continue;
      }
      const std::string prefab    = entry["prefab"].get<std::string>();
      const ActorId     id        = entry.value("id", ActorId{ 0 });
      const ActorId     root      = entry.value("root", id);
      const auto        overrides = entry.find("overrides");
      const auto        instance  = instances.find(root);
      nlohmann::json    resolved;
      if (instance != instances.end()) {
        resolved = ResolvePrefabNode(prefab, entry.value("node", ActorId{ 0 }), instance->second.ids, overrides != entry.end() ? &*overrides : nullptr);
      }
      if (resolved.is_null()) {
        Logger::Log("Prefab: actor %llu was skipped because the prefab \"%s\" (or the actor in it) no longer exists\n", static_cast<unsigned long long>(id), prefab.c_str());
        continue;
      }
      resolved["prefab"] = prefab;
      resolved["node"]   = entry["node"];
      if (root != id) {
        resolved["root"] = root;
      }
      expanded.push_back(std::move(resolved));
    }
    return expanded;
  }

  nlohmann::json CollapsePlacedActors(const nlohmann::json& entries)
  {
    // 組ごとに「プレハブの中の番号 → ID」を集める（組の中の親と ActorRef を、差分にしないため）
    std::map<ActorId, ActorIdMap> instances;
    for (const nlohmann::json& entry : entries) {
      if (entry.contains("prefab")) {
        const ActorId id = entry.value("id", ActorId{ 0 });
        instances[entry.value("root", id)][entry.value("node", ActorId{ 0 })] = id;
      }
    }

    nlohmann::json collapsed = nlohmann::json::array();
    for (const nlohmann::json& entry : entries) {
      nlohmann::json full = entry;
      ErasePrefabLink(full);
      const std::string prefab = entry.value("prefab", std::string{});
      const ActorId     id     = entry.value("id", ActorId{ 0 });
      const ActorId     node   = entry.value("node", ActorId{ 0 });
      const bool        isRoot = !entry.contains("root");
      const ActorIdMap& ids    = instances[entry.value("root", id)];
      // プレハブか、その中の番号が無くなっていたら、つながりを切って全部の値で書く
      const nlohmann::json base = prefab.empty() ? nlohmann::json() : ResolvePrefabNode(prefab, node, ids, nullptr);
      if (base.is_null()) {
        collapsed.push_back(std::move(full));
        continue;
      }

      nlohmann::json overrides = nlohmann::json::diff(base, full);
      nlohmann::json placed    = { { "id", id }, { "prefab", prefab }, { "node", node } };
      if (isRoot) {
        // 根の名前と Transform は、プレハブと同じ値でも持っておく（Apply でプレハブ側が変わっても、他の置いた物が動かないように）
        static const std::string kOwnPaths[] = { "/properties/name", "/properties/transform" };
        nlohmann::json kept = nlohmann::json::array();
        for (const nlohmann::json& operation : overrides) {
          const std::string path = operation.value("path", std::string{});
          if (std::ranges::none_of(kOwnPaths, [&path](const std::string& own) { return path.starts_with(own); })) {
            kept.push_back(operation);
          }
        }
        for (const std::string& own : kOwnPaths) {
          const nlohmann::json::json_pointer pointer{ own };
          if (full.contains(pointer)) {
            kept.push_back({ { "op", "replace" }, { "path", own }, { "value", full[pointer] } });
          }
        }
        overrides = std::move(kept);

        // 読むときに足りない Actor は必ず足しているので、今ここに居ない番号は消した物
        nlohmann::json removed = nlohmann::json::array();
        for (const ActorId existing : GetPrefabNodes(prefab)) {
          if (!ids.contains(existing)) { removed.push_back(existing); }
        }
        if (!removed.empty()) {
          placed["removed"] = std::move(removed);
        }
      } else {
        placed["root"] = entry["root"];
      }
      placed["overrides"] = std::move(overrides);
      collapsed.push_back(std::move(placed));
    }
    return collapsed;
  }

  nlohmann::json MakePrefabContent(std::string_view name, const nlohmann::json& tree, const ActorIdMap& nodes)
  {
    nlohmann::json actors   = tree.value("actors", nlohmann::json::array());
    const ActorId  treeRoot = actors.empty() ? ActorId{ 0 } : actors[0].value("id", ActorId{ 0 });

    // 各 Actor が、書き出すプレハブの中で指す別のプレハブ（root は書き出す番号）
    std::vector<PrefabLink> inner(actors.size());
    for (size_t i = 0; i < actors.size(); ++i) {
      nlohmann::json&   entry  = actors[i];
      const std::string prefab = entry.value("prefab", std::string{});
      const ActorId     id     = entry.value("id", ActorId{ 0 });
      const ActorId     root   = entry.value("root", id);
      if (prefab == name && root == treeRoot) {
        // Apply：今のファイルにある入れ子を保つ
        inner[i] = GetNestedPrefabLink(name, entry.value("node", ActorId{ 0 }));
      } else if (!prefab.empty() && nodes.contains(root) && !ContainsPrefab(prefab, name)) {
        inner[i] = { prefab, entry.value("node", ActorId{ 0 }), root == id ? ActorId{ 0 } : nodes.at(root) };
      }
      ErasePrefabLink(entry);
    }
    RemapActorIds(actors, nodes, false);

    for (size_t i = 0; i < actors.size(); ++i) {
      // 根が組から居なくなった入れ子の Actor は、写しにする
      if (inner[i].prefab.empty() || (inner[i].root != 0 && FindEntry(actors, inner[i].root) == actors.end())) {
        continue;
      }
      actors[i]["prefab"] = inner[i].prefab;
      actors[i]["node"]   = inner[i].node;
      if (inner[i].root != 0) {
        actors[i]["root"] = inner[i].root;
      }
    }
    return { { "actors", CollapsePlacedActors(actors) } };
  }

  bool SavePrefab(std::string_view name, const nlohmann::json& content)
  {
    std::filesystem::create_directories(kPrefabsDirectory);
    std::ofstream file(GetPrefabPath(name));
    if (!file) {
      return false;
    }
    file << content.dump(2);
    // 他のプレハブが中に置いているかもしれないので、全部を作り直す
    NormalizedCache().clear();
    return true;
  }

  nlohmann::json ResolvePrefabNode(std::string_view name, ActorId node, const ActorIdMap& ids, const nlohmann::json* overrides)
  {
    const nlohmann::json* prefab = FindNormalized(name);
    if (!prefab) {
      return nullptr;
    }
    const auto found = FindEntry(*prefab, node);
    if (found == prefab->end()) {
      return nullptr;
    }
    nlohmann::json merged = *found;
    // 中に置いたプレハブとのつながりは、置く側には渡さない（置いた物は外側のプレハブだけを指す）
    ErasePrefabLink(merged);
    RemapActorIds(merged, ids, false);
    if (overrides && overrides->is_array()) {
      for (const nlohmann::json& operation : *overrides) {
        try {
          merged = merged.patch(nlohmann::json::array({ operation }));
        } catch (const nlohmann::json::exception& e) {
          Logger::Log("Prefab: an override of \"%.*s\" no longer applies and was skipped: %s\n", static_cast<int>(name.size()), name.data(), e.what());
        }
      }
    }
    return merged;
  }

#ifdef _DEBUG
  void SetPrefabInMemory(std::string_view name, const nlohmann::json& content)
  {
    MemoryPrefabs().erase(std::string(name));
    if (!content.is_null()) {
      MemoryPrefabs().emplace(std::string(name), content);
    }
    NormalizedCache().clear();
  }
#endif

} // namespace Tako
