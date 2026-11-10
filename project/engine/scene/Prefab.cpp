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

    // ponytail: 読み直しはエディタで保存したときだけ。外でファイルを書き換えたら再起動で反映
    std::map<std::string, nlohmann::json, std::less<>>& NormalizedCache()
    {
      static std::map<std::string, nlohmann::json, std::less<>> cache;
      return cache;
    }

    /// <summary>
    /// プレハブの各 Actor を一度 Actor に読んで書き直した配列。差分を取る基準にする（float の誤差や、後から足した TPROPERTY を差分にしないため）
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
      nlohmann::json normalized = nlohmann::json::array();
      for (const nlohmann::json& entry : entries) {
        std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(entry.value("type", std::string("Actor")));
        if (!object || !object->IsA<Actor>() || entry.value("id", ActorId{ 0 }) == 0) {
          Logger::Log("Prefab: an actor of \"%.*s\" has an unknown type or no id and was skipped\n", static_cast<int>(name.size()), name.data());
          continue;
        }
        Actor& actor = static_cast<Actor&>(*object);
        actor.Load(entry);
        normalized.push_back(actor.Save());
      }
      return normalized;
    }

    const nlohmann::json* FindNormalized(std::string_view name)
    {
      auto& cache = NormalizedCache();
      if (auto it = cache.find(name); it != cache.end()) {
        return &it->second;
      }
      if (name.empty() || !std::filesystem::is_regular_file(GetPrefabPath(name))) {
        return nullptr;
      }
      nlohmann::json raw;
      try {
        std::ifstream file(GetPrefabPath(name));
        raw = nlohmann::json::parse(file);
      } catch (const nlohmann::json::exception& e) {
        Logger::Log("Prefab: failed to read %s: %s\n", GetPrefabPath(name).c_str(), e.what());
        return nullptr;
      }
      return &cache.emplace(std::string(name), Normalize(name, raw)).first->second;
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

    void ErasePrefabLink(nlohmann::json& entry)
    {
      entry.erase("prefab");
      entry.erase("node");
      entry.erase("root");
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

  nlohmann::json SaveActorTree(Actor& actor)
  {
    nlohmann::json actors = nlohmann::json::array();
    const auto add = [&actors](const auto& self, Actor& current) -> void {
      nlohmann::json entry = current.Save();
      if (!current.GetPrefab().empty()) {
        entry["prefab"] = current.GetPrefab();
        entry["node"]   = current.GetPrefabNode();
        if (!current.IsPrefabRoot()) {
          entry["root"] = current.GetPrefabRoot();
        }
      }
      actors.push_back(std::move(entry));
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

  nlohmann::json MakePrefabContent(const nlohmann::json& tree, const ActorIdMap& nodes)
  {
    nlohmann::json actors = tree.value("actors", nlohmann::json::array());
    for (nlohmann::json& entry : actors) {
      ErasePrefabLink(entry);
    }
    RemapActorIds(actors, nodes, false);
    return { { "actors", std::move(actors) } };
  }

  bool SavePrefab(std::string_view name, const nlohmann::json& content)
  {
    std::filesystem::create_directories(kPrefabsDirectory);
    std::ofstream file(GetPrefabPath(name));
    if (!file) {
      return false;
    }
    file << content.dump(2);
    NormalizedCache().erase(std::string(name));
    return true;
  }

  nlohmann::json SavePrefabInstance(Actor& actor, const ActorIdMap& ids)
  {
    nlohmann::json full = actor.Save();
    if (actor.GetPrefab().empty()) {
      return full;
    }
    const nlohmann::json base = ResolvePrefabNode(actor.GetPrefab(), actor.GetPrefabNode(), ids, nullptr);
    if (base.is_null()) {
      // プレハブか、その中の番号が無くなった。つながりを切って、全部の値で書く
      return full;
    }
    nlohmann::json overrides = nlohmann::json::diff(base, full);
    if (actor.IsPrefabRoot()) {
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
    }
    nlohmann::json entry = {
      { "id", full["id"] },
      { "prefab", actor.GetPrefab() },
      { "node", actor.GetPrefabNode() },
      { "overrides", std::move(overrides) },
    };
    if (!actor.IsPrefabRoot()) {
      entry["root"] = actor.GetPrefabRoot();
    }
    return entry;
  }

  nlohmann::json ResolvePrefabNode(std::string_view name, ActorId node, const ActorIdMap& ids, const nlohmann::json* overrides)
  {
    const nlohmann::json* prefab = FindNormalized(name);
    if (!prefab) {
      return nullptr;
    }
    const auto found = std::ranges::find_if(*prefab, [node](const nlohmann::json& entry) { return entry.value("id", ActorId{ 0 }) == node; });
    if (found == prefab->end()) {
      return nullptr;
    }
    nlohmann::json merged = *found;
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
    NormalizedCache().erase(std::string(name));
    if (!content.is_null()) {
      NormalizedCache().emplace(std::string(name), Normalize(name, content));
    }
  }
#endif

} // namespace Tako
