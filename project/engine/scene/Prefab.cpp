#include "Prefab.h"
#include "Actor.h"
#include "Logger.h"
#include "TypeRegistry.h"

#include <json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

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
    /// プレハブを一度 Actor に読んで書き直した内容。差分を取る基準にする（float の誤差や、後から足した TPROPERTY を差分にしないため）
    /// </summary>
    const nlohmann::json* FindNormalized(std::string_view name)
    {
      auto& cache = NormalizedCache();
      if (auto it = cache.find(name); it != cache.end()) {
        return &it->second;
      }
      if (!HasPrefab(name)) {
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
      std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(raw.value("type", std::string("Actor")));
      if (!object || !object->IsA<Actor>()) {
        return nullptr;
      }
      Actor& actor = static_cast<Actor&>(*object);
      actor.Load(raw);
      nlohmann::json normalized = actor.Save();
      normalized.erase("id");
      return &cache.emplace(std::string(name), std::move(normalized)).first->second;
    }

  } // namespace

  std::string GetPrefabPath(std::string_view name)
  {
    return std::string(kPrefabsDirectory) + std::string(name) + std::string(kPrefabExtension);
  }

  bool HasPrefab(std::string_view name)
  {
    return !name.empty() && std::filesystem::is_regular_file(GetPrefabPath(name));
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

  bool SavePrefab(std::string_view name, Actor& actor)
  {
    nlohmann::json json = actor.Save();
    json.erase("id");
    std::filesystem::create_directories(kPrefabsDirectory);
    std::ofstream file(GetPrefabPath(name));
    if (!file) {
      return false;
    }
    file << json.dump(2);
    NormalizedCache().erase(std::string(name));
    return true;
  }

  nlohmann::json SavePrefabInstance(Actor& actor)
  {
    nlohmann::json full = actor.Save();
    const nlohmann::json* prefab = FindNormalized(actor.GetPrefab());
    if (!prefab) {
      return full;
    }
    nlohmann::json base = *prefab;
    base["id"] = full["id"];
    return {
      { "id", full["id"] },
      { "prefab", actor.GetPrefab() },
      { "overrides", nlohmann::json::diff(base, full) },
    };
  }

  nlohmann::json ResolvePrefabInstance(const nlohmann::json& json)
  {
    auto prefabName = json.find("prefab");
    if (prefabName == json.end() || !prefabName->is_string()) {
      return json;
    }
    const nlohmann::json* prefab = FindNormalized(prefabName->get<std::string>());
    if (!prefab) {
      Logger::Log("Prefab: \"%s\" was not found\n", prefabName->get<std::string>().c_str());
      return json;
    }
    nlohmann::json merged = *prefab;
    if (auto id = json.find("id"); id != json.end()) {
      merged["id"] = *id;
    }
    if (auto overrides = json.find("overrides"); overrides != json.end() && overrides->is_array()) {
      for (const nlohmann::json& operation : *overrides) {
        try {
          merged = merged.patch(nlohmann::json::array({ operation }));
        } catch (const nlohmann::json::exception& e) {
          Logger::Log("Prefab: an override of \"%s\" no longer applies and was skipped: %s\n", prefabName->get<std::string>().c_str(), e.what());
        }
      }
    }
    return merged;
  }

} // namespace Tako
