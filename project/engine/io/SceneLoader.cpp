#include "SceneLoader.h"
#include "json.hpp"
#include "Logger.h"
#include "Object3d.h"

#include <fstream>
#include <cassert>

void SceneLoader::Initialize()
{
  directoryFolderName_ = "resources";
  secneFolderName_ = "Levels";
}

bool SceneLoader::LoadScene(const std::string& sceneFileName)
{
  /// ------------------------- ///
  /// jsonファイルをデシリアライズ ///
  /// ------------------------- ///

  // jsonファイルのパス
  std::string filePath = directoryFolderName_ + "/" + secneFolderName_ + "/" + sceneFileName + ".json";

  // ファイルストリーム
  std::ifstream file;

  // jsonファイルを開く
  file.open(filePath);
  // ファイルが開けなかったらassert
  if(file.fail())
  {
    Logger::Log("Failed to open scene file");
    assert(false);
  }

  nlohmann::json deserializedJson;

  // jsonファイルを読み込む
  file >> deserializedJson;


  if (deserializedJson.is_object())
  {
    Logger::Log("Deserialized JSON is not an object");
    assert(false);
  }
  if (deserializedJson.contains("name"))
  {
    Logger::Log("Deserialized JSON does not contain 'name' key");
    assert(false);
  }
  if (deserializedJson["name"].is_string())
  {
    Logger::Log("Deserialized JSON 'name' is not a string");
    assert(false);
  }

  /// ----------------------------- ///
  /// レベルデータを構造体に格納していく ///
  /// ----------------------------- ///

  LevelData* levelData = new LevelData;

  // name文字列を取得
  levelData->name = deserializedJson["name"].get<std::string>();

  for (nlohmann::json& object : deserializedJson["objects"])
  {
    assert(object.contains("type"));

    if (object["type"].get<std::string>() == "MESH")
    {
      // 1個分の要素の準備
      levelData->objects.emplace_back(ObjectData{});
      ObjectData& objectData = levelData->objects.back();

      // データを格納
      objectData.type = object["type"].get<std::string>();           // "type"
      objectData.name = object["name"].get<std::string>();           // "name"
      objectData.fileName = object["file_name"].get<std::string>();  // "fileName"

      // Transformデータを格納
      nlohmann::json& transform = object["transform"];
      // 平行移動 "translation"
      objectData.transform.translate.x = static_cast<float>(transform["translation"][0]);
      objectData.transform.translate.y = static_cast<float>(transform["translation"][2]);
      objectData.transform.translate.z = static_cast<float>(transform["translation"][1]);
      // 回転 "rotation"
      objectData.transform.rotate.x    = -static_cast<float>(transform["rotation"][0]);
      objectData.transform.rotate.y    = -static_cast<float>(transform["rotation"][2]);
      objectData.transform.rotate.z    = -static_cast<float>(transform["rotation"][1]);
      // 拡大縮小 "scale"
      objectData.transform.scale.x     = static_cast<float>(transform["scale"][0]);
      objectData.transform.scale.y     = static_cast<float>(transform["scale"][2]);
      objectData.transform.scale.z     = static_cast<float>(transform["scale"][1]);
    }
  }

  /// ---------------------------------- ///
  /// レベルデータからオブジェクトを生成、配置 ///
  /// ---------------------------------- ///

  for (auto& objectData : levelData->objects)
  {
    if (objectData.fileName.empty())
    {
      Logger::Log("Object file name is empty");
      assert(false);
    }
    // オブジェクトを生成
  }

}
