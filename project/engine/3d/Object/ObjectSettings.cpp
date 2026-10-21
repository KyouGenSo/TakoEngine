#include "ObjectSettings.h"

#include <filesystem>
#include <fstream>
#include <iterator>

#include <json.hpp>

#include "Logger.h"
#include "MaterialLibrary.h"

namespace Tako {

  using json = nlohmann::json;

  namespace {

    const std::filesystem::path kSettingsPath = "resources/Json/ObjectSettings.json";

    template<class T>
    void Read(const json& entry, const char* key, std::optional<T>& value)
    {
      if (const auto it = entry.find(key); it != entry.end()) {
        value = it->get<T>();
      }
    }

    template<class T>
    void Write(json& entry, const char* key, const std::optional<T>& value)
    {
      if (value) {
        entry[key] = *value;
      }
    }

    void ReadMaterial(const json& entry, MaterialOverrides& overrides)
    {
      Read(entry, "Material", overrides.material);
      if (const auto it = entry.find("Values"); it != entry.end() && it->is_object()) {
        overrides.values = *it;
      }
      overrides.textures = entry.value("Textures", std::map<std::string, std::string>{});
    }

    void WriteMaterial(json& entry, const MaterialOverrides& overrides)
    {
      Write(entry, "Material", overrides.material);
      if (!overrides.values.empty()) {
        entry["Values"] = overrides.values;
      }
      if (!overrides.textures.empty()) {
        entry["Textures"] = overrides.textures;
      }
    }

    bool HasMaterialSettings(const MaterialOverrides& overrides)
    {
      return overrides.material || !overrides.values.empty() || !overrides.textures.empty();
    }

    ResolvedMaterial ResolveMaterial(const MaterialOverrides& overrides)
    {
      ResolvedMaterial resolved;
      if (overrides.material && !overrides.material->empty()) {
        resolved.material = MaterialLibrary::GetInstance()->Load(*overrides.material);
      }
      resolved.properties.SetValues(overrides.values);
      // 空のファイルはまだ指定していない欄なので当てない（white.png にしない）
      for (const auto& [slot, file] : overrides.textures) {
        if (!file.empty()) {
          resolved.properties.SetTexture(slot, MaterialLibrary::ResolveTexture(file));
        }
      }
      return resolved;
    }

  } // anonymous namespace

  const ResolvedMaterial& ObjectOverrides::Resolve(std::string_view mesh) const
  {
    const auto it = resolvedMeshes.find(mesh);
    return it != resolvedMeshes.end() ? it->second : resolved;
  }

  std::unique_ptr<ObjectSettings> ObjectSettings::instance_ = nullptr;

  ObjectSettings* ObjectSettings::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<ObjectSettings>(Token{});
    }
    return instance_.get();
  }

  void ObjectSettings::Load()
  {
    std::ifstream ifs(kSettingsPath);
    if (!ifs) {
      return;
    }
    Apply(std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()));
  }

  void ObjectSettings::Finalize()
  {
    instance_.reset();
  }

  bool ObjectSettings::Save() const
  {
    std::error_code ec;
    std::filesystem::create_directories(kSettingsPath.parent_path(), ec);
    std::ofstream ofs(kSettingsPath);
    if (!ofs) {
      Logger::Log("Object settings save failed: %s", kSettingsPath.string().c_str());
      return false;
    }
    ofs << Serialize() << std::endl;
    return true;
  }

  std::string ObjectSettings::Serialize() const
  {
    json root = json::object();
    for (const auto& [name, overrides] : entries_) {
      json entry = json::object();
      Write(entry, "Visible", overrides.visible);
      WriteMaterial(entry, overrides);
      Write(entry, "MaskId", overrides.maskId);
      Write(entry, "SortingOrder", overrides.sortingOrder);
      Write(entry, "CastShadow", overrides.castShadow);
      Write(entry, "Transparent", overrides.transparent);
      json meshes = json::object();
      for (const auto& [meshName, mesh] : overrides.meshes) {
        json meshEntry = json::object();
        Write(meshEntry, "Visible", mesh.visible);
        WriteMaterial(meshEntry, mesh);
        if (!meshEntry.empty()) {
          meshes[meshName] = std::move(meshEntry);
        }
      }
      if (!meshes.empty()) {
        entry["Meshes"] = std::move(meshes);
      }
      if (!entry.empty()) {
        root[name] = std::move(entry);
      }
    }
    return root.dump(4);
  }

  bool ObjectSettings::Apply(const std::string& text)
  {
    std::map<std::string, ObjectOverrides, std::less<>> entries;
    try {
      // items() は元の JSON を参照するので、回す前に受けておく
      const json root = json::parse(text);
      for (const auto& [name, entry] : root.items()) {
        ObjectOverrides& overrides = entries[name];
        Read(entry, "Visible", overrides.visible);
        ReadMaterial(entry, overrides);
        Read(entry, "MaskId", overrides.maskId);
        Read(entry, "SortingOrder", overrides.sortingOrder);
        Read(entry, "CastShadow", overrides.castShadow);
        Read(entry, "Transparent", overrides.transparent);
        const json meshes = entry.value("Meshes", json::object());
        for (const auto& [meshName, meshEntry] : meshes.items()) {
          MeshOverrides& mesh = overrides.meshes[meshName];
          Read(meshEntry, "Visible", mesh.visible);
          ReadMaterial(meshEntry, mesh);
        }
      }
    }
    catch (const json::exception& e) {
      Logger::Log("Object settings load failed: %s", e.what());
      return false;
    }
    entries_ = std::move(entries);
    MarkChanged();
    return true;
  }

  ObjectOverrides& ObjectSettings::Edit(const std::string& name)
  {
    return entries_[name];
  }

  void ObjectSettings::MarkChanged()
  {
    for (auto& [name, overrides] : entries_) {
      overrides.resolved = ResolveMaterial(overrides);
      overrides.resolvedMeshes.clear();
      for (const auto& [meshName, mesh] : overrides.meshes) {
        if (!HasMaterialSettings(mesh)) {
          continue;
        }
        // 全メッシュ用の設定に、メッシュ名の設定を重ねる
        MaterialOverrides merged = static_cast<const MaterialOverrides&>(overrides);
        if (mesh.material) {
          merged.material = mesh.material;
        }
        merged.values.update(mesh.values);
        for (const auto& [slot, file] : mesh.textures) {
          merged.textures.insert_or_assign(slot, file);
        }
        overrides.resolvedMeshes.emplace(meshName, ResolveMaterial(merged));
      }
    }
    ++version_;
  }


  const ObjectOverrides* ObjectSettings::Find(std::string_view name) const
  {
    const auto it = entries_.find(name);
    return it != entries_.end() ? &it->second : nullptr;
  }

  std::vector<std::string> ObjectSettings::GetNames() const
  {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto& [name, overrides] : entries_) {
      names.push_back(name);
    }
    return names;
  }

} // namespace Tako
