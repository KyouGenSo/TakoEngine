#include "SceneLoader.h"
#include "json.hpp"
#include "Logger.h"
#include "Object3d.h"

#include <fstream>
#include <cassert>
#include <DirectXMath.h>

/// <summary>
/// LoadedSceneクラスの実装
/// </summary>

Object3d* SceneLoader::LoadedScene::GetObject3d(const std::string& name)
{
  auto it = objectMap_.find(name);
  if (it != objectMap_.end())
  {
    return it->second;
  }
  return nullptr;
}

void SceneLoader::LoadedScene::AddObject(const std::string& name, std::unique_ptr<Object3d> object)
{
  Object3d* rawPtr = object.get();
  objects_.push_back(std::move(object));
  objectMap_[name] = rawPtr;
}

void SceneLoader::LoadedScene::Clear()
{
  objects_.clear();
  objectMap_.clear();
}

void SceneLoader::Initialize()
{
  directoryFolderName_ = "resources/Json";
  secneFolderName_ = "Levels";
}

std::unique_ptr<SceneLoader::LoadedScene> SceneLoader::LoadScene(const std::string& sceneFileName)
{
  // LoadedSceneを作成
  auto loadedScene = std::make_unique<LoadedScene>();
  /// ------------------------- ///
  /// jsonファイルをデシリアライズ ///
  /// ------------------------- ///

  // jsonファイルのパス
  std::string filePath = directoryFolderName_ + "/" + secneFolderName_ + "/" + sceneFileName + ".json";

  // ファイルストリーム
  std::ifstream file;

  // jsonファイルを開く
  file.open(filePath);
  // ファイルが開けなかったらnullptrを返す
  if(file.fail())
  {
    Logger::Log("Failed to open scene file");
    return nullptr;
  }

  nlohmann::json deserializedJson;

  // jsonファイルを読み込む
  file >> deserializedJson;


  if (!deserializedJson.is_object())
  {
    Logger::Log("Deserialized JSON is not an object");
    return nullptr;
  }
  if (!deserializedJson.contains("name"))
  {
    Logger::Log("Deserialized JSON does not contain 'name' key");
    return nullptr;
  }
  if (!deserializedJson["name"].is_string())
  {
    Logger::Log("Deserialized JSON 'name' is not a string");
    return nullptr;
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

    if (object["type"].get<std::string>() == "MESH" && object.contains("file_name"))
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
      objectData.transform.translate.x = static_cast<float>(transform["translation"][1]);
      objectData.transform.translate.y = static_cast<float>(transform["translation"][2]);
      objectData.transform.translate.z = -static_cast<float>(transform["translation"][0]);
      // 回転 "rotation" (度からラジアンに変換)
      objectData.transform.rotate.x    = DirectX::XMConvertToRadians(static_cast<float>(transform["rotation"][2]));
      objectData.transform.rotate.y    = DirectX::XMConvertToRadians(static_cast<float>(transform["rotation"][0]));
      objectData.transform.rotate.z    = DirectX::XMConvertToRadians(static_cast<float>(transform["rotation"][1]));
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
      continue; // エラー時はスキップ
    }
    
    // Object3dのインスタンスを生成
    auto object3d = std::make_unique<Object3d>();
    
    // 初期化
    object3d->Initialize();
    
    // モデルを設定
    object3d->SetModel(objectData.fileName);
    
    // Transformを設定
    object3d->SetTransform(objectData.transform);
    
    // LoadedSceneに追加
    loadedScene->AddObject(objectData.name, std::move(object3d));
  }

  // levelDataを削除
  delete levelData;

  // 読み込んだシーンを返す
  return loadedScene;
}
