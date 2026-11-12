#include "ObjectOverrides.h"

#include <json.hpp>

#include "MaterialLibrary.h"

namespace Tako {

  using json = nlohmann::json;

  namespace {

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

  json ObjectOverrides::Save() const
  {
    json entry = json::object();
    Write(entry, "Visible", visible);
    WriteMaterial(entry, *this);
    Write(entry, "MaskId", maskId);
    Write(entry, "SortingOrder", sortingOrder);
    Write(entry, "CastShadow", castShadow);
    Write(entry, "Transparent", transparent);
    Write(entry, "PostEffect", postEffect);
    json meshEntries = json::object();
    for (const auto& [meshName, mesh] : meshes) {
      json meshEntry = json::object();
      Write(meshEntry, "Visible", mesh.visible);
      WriteMaterial(meshEntry, mesh);
      if (!meshEntry.empty()) {
        meshEntries[meshName] = std::move(meshEntry);
      }
    }
    if (!meshEntries.empty()) {
      entry["Meshes"] = std::move(meshEntries);
    }
    return entry;
  }

  ObjectOverrides ObjectOverrides::Load(const json& entry)
  {
    ObjectOverrides overrides;
    Read(entry, "Visible", overrides.visible);
    ReadMaterial(entry, overrides);
    Read(entry, "MaskId", overrides.maskId);
    Read(entry, "SortingOrder", overrides.sortingOrder);
    Read(entry, "CastShadow", overrides.castShadow);
    Read(entry, "Transparent", overrides.transparent);
    Read(entry, "PostEffect", overrides.postEffect);
    // items() は元の JSON を参照するので、回す前に受けておく
    const json meshEntries = entry.value("Meshes", json::object());
    for (const auto& [meshName, meshEntry] : meshEntries.items()) {
      MeshOverrides& mesh = overrides.meshes[meshName];
      Read(meshEntry, "Visible", mesh.visible);
      ReadMaterial(meshEntry, mesh);
    }
    return overrides;
  }

  void ObjectOverrides::ResolveMaterials()
  {
    resolved = ResolveMaterial(*this);
    resolvedMeshes.clear();
    for (const auto& [meshName, mesh] : meshes) {
      if (!HasMaterialSettings(mesh)) {
        continue;
      }
      // 全メッシュ用の設定に、メッシュ名の設定を重ねる
      MaterialOverrides merged = static_cast<const MaterialOverrides&>(*this);
      if (mesh.material) {
        merged.material = mesh.material;
      }
      merged.values.update(mesh.values);
      for (const auto& [slot, file] : mesh.textures) {
        merged.textures.insert_or_assign(slot, file);
      }
      resolvedMeshes.emplace(meshName, ResolveMaterial(merged));
    }
  }

} // namespace Tako
