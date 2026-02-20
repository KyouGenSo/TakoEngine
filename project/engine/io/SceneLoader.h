#pragma once
#include "Transform.h"

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace Tako {

class Object3d;

class SceneLoader
{
public: // 構造体定義
  /// <summary>
  /// 読み込まれたシーンを保持するコンテナ構造体
  /// </summary>
  struct LoadedScene
  {
  public:
    /// <summary>
    /// 名前で Object3d を取得
    /// </summary>
    /// <param name="name">オブジェクト名</param>
    /// <returns>Object3d のポインタ（見つからない場合は nullptr）</returns>
    Object3d* GetObject3d(const std::string& name);

    /// <summary>
    /// Object3d を追加
    /// </summary>
    /// <param name="name">オブジェクト名</param>
    /// <param name="object">Object3d の unique_ptr</param>
    void AddObject(const std::string& name, std::unique_ptr<Object3d> object);

    /// <summary>
    /// 全ての Object3d を取得
    /// </summary>
    /// <returns>Object3d のリスト</returns>
    const std::vector<std::unique_ptr<Object3d>>& GetAllObjects() const { return objects_; }

    /// <summary>
    /// クリア
    /// </summary>
    void Clear();

  private:
    std::vector<std::unique_ptr<Object3d>> objects_; ///< オブジェクトのリスト
    std::map<std::string, Object3d*> objectMap_; ///< 名前からオブジェクトへのマップ
  };

  /// <summary>
  /// オブジェクトデータ構造体
  /// </summary>
  struct ObjectData
  {
    std::string type; ///< オブジェクトタイプ（例: "Mesh", "Light", "Camera"）
    std::string name; ///< オブジェクト名
    Transform transform; ///< トランスフォーム（位置、回転、スケール）
    std::string fileName; ///< ファイル名
  };

  /// <summary>
  /// レベルデータ構造体
  /// </summary>
  struct LevelData
  {
    std::string name; ///< レベル名
    std::vector<ObjectData> objects; ///< レベル内のオブジェクトリスト
  };

public: //　メンバー関数
  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize();

  /// <summary>
  /// シーンを json ファイルから読み込む
  /// </summary>
  /// <param name="sceneFileName">シーンファイル名</param>
  /// <returns>読み込まれたシーンデータ（失敗時は nullptr）</returns>
  std::unique_ptr<LoadedScene> LoadScene(const std::string& sceneFileName);

  // -----------------------------------Setters-----------------------------------//
  /// <summary>
  /// ディレクトリフォルダ名を設定
  /// </summary>
  /// <param name="directoryFolderName">ディレクトリフォルダ名</param>
  void SetDirectoryFolderName(const std::string& directoryFolderName)
  {
    directoryFolderName_ = directoryFolderName;
  }

  /// <summary>
  /// シーンフォルダ名を設定
  /// </summary>
  /// <param name="scenefolderName">シーンフォルダ名</param>
  void SetSecneFolderName(const std::string& scenefolderName)
  {
    secneFolderName_ = scenefolderName;
  }

private:
  std::string directoryFolderName_; ///< ディレクトリフォルダ名

  std::string secneFolderName_; ///< シーンフォルダ名
};

} // namespace Tako