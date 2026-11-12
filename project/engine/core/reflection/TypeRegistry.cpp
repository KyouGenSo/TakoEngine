#include "TypeRegistry.h"
#include "Logger.h"

namespace Tako {

  bool TypeInfo::IsChildOf(const TypeInfo& other) const
  {
    for (const TypeInfo* type = this; type; type = type->base) {
      if (type == &other) { return true; }
    }
    return false;
  }

  TypeRegistry* TypeRegistry::GetInstance()
  {
    static TypeRegistry instance;
    return &instance;
  }

  void TypeRegistry::Register(const TypeInfo& type)
  {
    auto [it, inserted] = types_.try_emplace(type.name, &type);
    if (!inserted && it->second != &type) {
      Logger::Log("TypeRegistry: type name \"%s\" is used by two classes. The second one is ignored\n", type.name);
    }
  }

  void TypeRegistry::Unregister(std::string_view name)
  {
    if (auto it = types_.find(name); it != types_.end()) {
      types_.erase(it);
    }
  }

  std::unique_ptr<TObject> TypeRegistry::Create(std::string_view name) const
  {
    const TypeInfo* type = Find(name);
    return type && type->create ? type->create() : nullptr;
  }

  const TypeInfo* TypeRegistry::Find(std::string_view name) const
  {
    auto it = types_.find(name);
    return it != types_.end() ? it->second : nullptr;
  }

  std::vector<const TypeInfo*> TypeRegistry::GetCreatableTypes(const TypeInfo& base) const
  {
    std::vector<const TypeInfo*> result;
    for (const auto& [name, type] : types_) {
      if (type->create && type->IsChildOf(base)) { result.push_back(type); }
    }
    return result;
  }

  std::vector<std::string> TypeRegistry::GetTypeNames() const
  {
    std::vector<std::string> names;
    for (const auto& [name, type] : types_) {
      names.push_back(name);
    }
    return names;
  }

} // namespace Tako
