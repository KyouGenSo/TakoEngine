#include "SceneLoader.h"
#include "json.hpp"

#include <fstream>
#include <cassert>

void SceneLoader::Initialize()
{
  directoryFolderName_ = "resources";
  secneFolderName_ = "Levels";
}

bool SceneLoader::LoadScene(const std::string& sceneName)
{
  /// ------------------------- ///
  /// jsonファイルをデシリアライズ ///
  /// ------------------------- ///

  // jsonファイルのパス
  std::string filePath = directoryFolderName_ + "/" + secneFolderName_ + "/" + sceneName + ".json";

  // ファイルストリーム
  std::ifstream file;

  // jsonファイルを開く
  file.open(filePath);
  // ファイルが開けなかったらassert
  if(file.fail())
  {
    assert(false && "Failed to open scene file");
  }

  nlohmann::json deserializedJson;

  // jsonファイルを読み込む
  file >> deserializedJson;

  assert(deserializedJson.is_object() && "Deserialized JSON is not an object");
  assert(deserializedJson.contains("name") && "Deserialized JSON does not contain 'name' key");
  assert(deserializedJson["name"].is_string() && "Deserialized JSON 'name' is not a string");
}
