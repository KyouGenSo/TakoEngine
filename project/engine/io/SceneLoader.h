#pragma once
#include "Transform.h"

#include <string>
#include <vector>
#include <memory>
#include <map>

class Object3d;

class SceneLoader
{
public: // 構造体定義
  // LoadedSceneコンテナ構造体
  struct LoadedScene
  {
  public:
    /// <summary>
    /// 名前でObject3dを取得
    /// </summary>
    /// <param name="name">オブジェクト名</param>
    /// <returns>Object3dのポインタ（見つからない場合はnullptr）</returns>
    Object3d* GetObject3d(const std::string& name);

    /// <summary>
    /// Object3dを追加
    /// </summary>
    /// <param name="name">オブジェクト名</param>
    /// <param name="object">Object3dのunique_ptr</param>
    void AddObject(const std::string& name, std::unique_ptr<Object3d> object);

    /// <summary>
    /// 全てのObject3dを取得
    /// </summary>
    /// <returns>Object3dのリスト</returns>
    const std::vector<std::unique_ptr<Object3d>>& GetAllObjects() const { return objects_; }

    /// <summary>
    /// クリア
    /// </summary>
    void Clear();

  private:
    std::vector<std::unique_ptr<Object3d>> objects_;
    std::map<std::string, Object3d*> objectMap_;
  };

  struct ObjectData
  {
    std::string type; // e.g., "Mesh", "Light", "Camera"
    std::string name;
    Transform transform; // Position, rotation, scale
    std::string fileName;
  };

  struct LevelData
  {
    std::string name;
    std::vector<ObjectData> objects; // List of objects in the level
  };

public: //　メンバー関数
  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize();

  /// <summary>
  /// シーンをjsonファイルから読み込む
  /// </summary>
  /// <param name="sceneFileName">シーンファイル名</param>
  /// <returns>読み込まれたシーンデータ（失敗時はnullptr）</returns>
  std::unique_ptr<LoadedScene> LoadScene(const std::string& sceneFileName);

  // -----------------------------------Setters-----------------------------------//
  void SetDirectoryFolderName(const std::string& directoryFolderName)
  {
    directoryFolderName_ = directoryFolderName;
  }
  void SetSecneFolderName(const std::string& scenefolderName)
  {
    secneFolderName_ = scenefolderName;
  }

private:
  std::string directoryFolderName_;

  std::string secneFolderName_;
};