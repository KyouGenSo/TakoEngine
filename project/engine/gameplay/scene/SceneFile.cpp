#include "SceneFile.h"
#include "BaseScene.h"
#include "Logger.h"

#include <json.hpp>

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace Tako::SceneFile {

  namespace {

    namespace fs = std::filesystem;

    constexpr const char* kOrderFile = "order.txt";
    constexpr int         kFormat    = 2;

    // 先に書くキー。Actor の ID・型・プレハブとのつながり・名前（properties の中）が、ファイルの頭で読める
    constexpr const char* kLeadingKeys[] = { "id", "type", "prefab", "node", "root", "parent", "removed", "order", "properties" };

    bool IsLeadingKey(const std::string& key)
    {
      return std::ranges::any_of(kLeadingKeys, [&key](const char* leading) { return key == leading; });
    }

    void AppendNumber(std::string& out, double value)
    {
      if (!std::isfinite(value)) {
        out += "null";
        return;
      }
      // float から来た値は、float の最短の表記で書く（0.2800000011920929 ではなく 0.28）。読み戻すと同じ float になる。
      // float で正確に表せる double も同じ扱いなので、double として読む相手には 1e-8 ほど違う値になる
      char        buffer[32];
      const bool  fitsFloat = std::abs(value) <= FLT_MAX && static_cast<double>(static_cast<float>(value)) == value;
      const auto  result    = fitsFloat ? std::to_chars(buffer, buffer + sizeof(buffer), static_cast<float>(value)) : std::to_chars(buffer, buffer + sizeof(buffer), value);
      const std::string_view text(buffer, static_cast<size_t>(result.ptr - buffer));
      out += text;
      // 「1」と書くと、読み戻したときに整数になる
      if (text.find_first_of(".e") == std::string_view::npos) {
        out += ".0";
      }
    }

    void AppendValue(std::string& out, const nlohmann::json& value, int depth)
    {
      const auto indent = [&out](int level) { out.append(static_cast<size_t>(level) * 2, ' '); };

      if (value.is_object()) {
        if (value.empty()) {
          out += "{}";
          return;
        }
        bool       isFirst = true;
        const auto member  = [&](const std::string& key, const nlohmann::json& child) {
          out += isFirst ? "{\n" : ",\n";
          isFirst = false;
          indent(depth + 1);
          out += nlohmann::json(key).dump();
          out += ": ";
          AppendValue(out, child, depth + 1);
        };
        for (const char* key : kLeadingKeys) {
          if (auto it = value.find(key); it != value.end()) { member(it.key(), *it); }
        }
        for (auto it = value.begin(); it != value.end(); ++it) {
          if (!IsLeadingKey(it.key())) { member(it.key(), *it); }
        }
        out += "\n";
        indent(depth);
        out += "}";
        return;
      }

      if (value.is_array()) {
        const bool isInline = std::ranges::all_of(value, [](const nlohmann::json& element) { return element.is_primitive(); });
        out += "[";
        for (size_t i = 0; i < value.size(); ++i) {
          if (isInline) {
            out += i == 0 ? "" : ", ";
          } else {
            out += i == 0 ? "\n" : ",\n";
            indent(depth + 1);
          }
          AppendValue(out, value[i], depth + 1);
        }
        if (!isInline) {
          out += "\n";
          indent(depth);
        }
        out += "]";
        return;
      }

      if (value.is_number_float()) {
        AppendNumber(out, value.get<double>());
        return;
      }
      out += value.dump();
    }

    std::string ReadAll(std::ifstream& file)
    {
      return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    /// <summary>
    /// 中身が同じなら書かない（保存のたびに、変えていない Actor のファイルまで書き直さない）
    /// </summary>
    bool WriteText(const fs::path& path, const std::string& text)
    {
      if (std::ifstream existing(path); existing && ReadAll(existing) == text) {
        return true;
      }
      std::ofstream file(path);
      file << text;
      return static_cast<bool>(file);
    }

    fs::path ActorFileOf(const fs::path& folder, ActorId id)
    {
      return folder / (std::to_string(id) + ".json");
    }

    /// <summary>
    /// 名前.scene.json → 名前.scene
    /// </summary>
    fs::path FolderOf(const fs::path& sceneFile)
    {
      return fs::path(sceneFile).replace_extension();
    }

#ifdef TAKO_EDITOR
    /// <summary>
    /// 「ID.json」の ID。Actor のファイルの名前でなければ 0
    /// </summary>
    ActorId ActorIdOf(const fs::path& file)
    {
      const std::string stem = file.stem().string();
      ActorId           id   = 0;
      const auto        result = std::from_chars(stem.data(), stem.data() + stem.size(), id);
      return file.extension() == ".json" && result.ec == std::errc{} && result.ptr == stem.data() + stem.size() ? id : 0;
    }
#endif

  } // namespace

  std::string ToText(const nlohmann::json& json)
  {
    std::string text;
    AppendValue(text, json, 0);
    text += "\n";
    return text;
  }

  std::vector<ActorId> ParseOrder(std::string_view text)
  {
    std::vector<ActorId>        ids;
    std::unordered_set<ActorId> seen;
    while (!text.empty()) {
      const size_t     end  = text.find('\n');
      std::string_view line = text.substr(0, end);
      text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      ActorId    id     = 0;
      const auto result = std::from_chars(line.data(), line.data() + line.size(), id);
      if (result.ec == std::errc{} && result.ptr == line.data() + line.size() && id != 0 && seen.insert(id).second) {
        ids.push_back(id);
      }
    }
    return ids;
  }

  nlohmann::json ReadActors(const fs::path& folder)
  {
    std::ifstream              orderFile(folder / kOrderFile);
    const std::vector<ActorId> order  = ParseOrder(ReadAll(orderFile));
    nlohmann::json             actors = nlohmann::json::array();
    for (const ActorId id : order) {
      const fs::path path = ActorFileOf(folder, id);
      std::ifstream  file(path);
      // 片方のブランチが消した Actor の行が、マージで残っただけ
      if (!file) {
        continue;
      }
      nlohmann::json entry = nlohmann::json::parse(file, nullptr, false);
      if (!entry.is_object()) {
        Logger::Log("Scene: %s is not valid JSON and was skipped. Fix the file (or delete it) before saving the scene\n", path.generic_string().c_str());
        continue;
      }
      // ファイルを手で写して増やした Actor は、中の ID が元のまま
      entry["id"] = id;
      actors.push_back(std::move(entry));
    }

#ifdef TAKO_EDITOR
    // order.txt に無い Actor のファイルは読まない。片方が消して片方が直した Actor をマージで残した、などに気付けるように出す
    const std::unordered_set<ActorId> listed(order.begin(), order.end());
    std::error_code                   error;
    for (const fs::directory_entry& item : fs::directory_iterator(folder, error)) {
      if (const ActorId id = ActorIdOf(item.path()); id != 0 && !listed.contains(id)) {
        Logger::Log("Scene: %s is not listed in %s and was not loaded. Add its id to the list, or delete the file\n", item.path().generic_string().c_str(), kOrderFile);
      }
    }
#endif
    return actors;
  }

  bool WriteActors(const fs::path& folder, const nlohmann::json& actors)
  {
    std::unordered_set<ActorId> current;
    for (const nlohmann::json& entry : actors) {
      current.insert(entry.value("id", ActorId{ 0 }));
    }

    std::vector<fs::path> removed;
    {
      std::ifstream orderFile(folder / kOrderFile);
      for (const ActorId id : ParseOrder(ReadAll(orderFile))) {
        if (current.contains(id)) {
          continue;
        }
        const fs::path path = ActorFileOf(folder, id);
        std::ifstream  file(path);
        if (!file) {
          continue;
        }
        // 読めないファイルの Actor は、シーンに入っていない。消したのではなく、直す前かもしれない
        if (!nlohmann::json::parse(file, nullptr, false).is_object()) {
          Logger::Log("Scene: not saved, because %s is not valid JSON. Fix the file (or delete it) first\n", path.generic_string().c_str());
          return false;
        }
        removed.push_back(path);
      }
    }

    std::error_code error;
    fs::create_directories(folder, error);
    std::string order;
    for (const nlohmann::json& entry : actors) {
      const ActorId id = entry.value("id", ActorId{ 0 });
      if (id == 0 || !WriteText(ActorFileOf(folder, id), ToText(entry))) {
        return false;
      }
      order += std::to_string(id) + "\n";
    }
    if (!WriteText(folder / kOrderFile, order)) {
      return false;
    }
    for (const fs::path& path : removed) {
      fs::remove(path, error);
    }
    return true;
  }

  nlohmann::json Read(std::string_view sceneName)
  {
    const fs::path       sceneFile = BaseScene::GetScenePath(sceneName);
    std::ifstream        file(sceneFile);
    const nlohmann::json scene = nlohmann::json::parse(file);
    if (scene.contains("actors")) {
      std::error_code error;
      if (fs::exists(FolderOf(sceneFile), error)) {
        Logger::Log("Scene: %s holds the actors itself, so the folder %s was ignored\n", sceneFile.generic_string().c_str(), FolderOf(sceneFile).generic_string().c_str());
      }
      return scene;
    }
    return { { "actors", ReadActors(FolderOf(sceneFile)) } };
  }

  bool Write(std::string_view sceneName, const nlohmann::json& scene)
  {
    const fs::path sceneFile = BaseScene::GetScenePath(sceneName);
    bool           isSplit   = false;
    if (std::ifstream file(sceneFile); file) {
      const nlohmann::json existing = nlohmann::json::parse(file, nullptr, false);
      // 読めないシーンは、空で開いている。そのまま書くと、Actor のファイルを全部消すことになる
      if (existing.is_discarded()) {
        Logger::Log("Scene: not saved, because %s is not valid JSON. Fix the file first\n", sceneFile.generic_string().c_str());
        return false;
      }
      isSplit = !existing.contains("actors");
    }

    std::error_code error;
    fs::create_directories(sceneFile.parent_path(), error);
    const auto actors = scene.find("actors");
    if (!WriteActors(FolderOf(sceneFile), actors != scene.end() ? *actors : nlohmann::json::array())) {
      return false;
    }
    // 最後に書く。途中で止まっても、全部を 1 ファイルに書いた形が残っていて開ける
    return isSplit || WriteText(sceneFile, ToText({ { "format", kFormat } }));
  }

} // namespace Tako::SceneFile
