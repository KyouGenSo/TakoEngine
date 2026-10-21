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

  } // anonymous namespace

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
      Write(entry, "Material", overrides.material);
      Write(entry, "MaskId", overrides.maskId);
      Write(entry, "SortingOrder", overrides.sortingOrder);
      Write(entry, "CastShadow", overrides.castShadow);
      Write(entry, "Transparent", overrides.transparent);
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
        Read(entry, "Material", overrides.material);
        Read(entry, "MaskId", overrides.maskId);
        Read(entry, "SortingOrder", overrides.sortingOrder);
        Read(entry, "CastShadow", overrides.castShadow);
        Read(entry, "Transparent", overrides.transparent);
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
    MaterialLibrary* library = MaterialLibrary::GetInstance();
    for (auto& [name, overrides] : entries_) {
      overrides.loadedMaterial = overrides.material && !overrides.material->empty() ? library->Load(*overrides.material) : nullptr;
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
