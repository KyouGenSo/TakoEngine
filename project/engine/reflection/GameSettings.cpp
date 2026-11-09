#include "GameSettings.h"
#include "JsonPropertyVisitor.h"
#include "Logger.h"

#include <json.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

namespace Tako {

  GameSettings& GameSettings::Get(const TypeInfo& type)
  {
    static std::map<const TypeInfo*, std::unique_ptr<GameSettings>> instances;
    std::unique_ptr<GameSettings>& instance = instances[&type];
    if (!instance) {
      std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(type.name);
      assert(object && object->IsA<GameSettings>() && "GameSettings の作れる派生を渡す");
      instance.reset(static_cast<GameSettings*>(object.release()));
      instance->Reload();
    }
    return *instance;
  }

  bool GameSettings::Save()
  {
    const std::filesystem::path path = GetPath(GetType());
    std::error_code             ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream ofs(path);
    if (!ofs) {
      return false;
    }
    ofs << WriteProperties(*this).dump(4) << std::endl;
    return static_cast<bool>(ofs);
  }

  void GameSettings::Reload()
  {
    std::ifstream ifs(GetPath(GetType()));
    if (!ifs) {
      return;
    }
    // 手で書き換えたファイルの書き間違いは、値を変えずに知らせるだけにする
    const nlohmann::json json = nlohmann::json::parse(ifs, nullptr, false);
    if (json.is_discarded()) {
      Logger::Log("GameSettings: failed to parse " + GetPath(GetType()) + "\n");
      return;
    }
    ReadProperties(*this, json);
  }

  std::vector<const TypeInfo*> GameSettings::GetTypes()
  {
    return TypeRegistry::GetInstance()->GetCreatableTypes(StaticType());
  }

  std::string GameSettings::GetPath(const TypeInfo& type)
  {
    return std::string("resources/Json/GameSettings/") + type.name + ".json";
  }

} // namespace Tako
