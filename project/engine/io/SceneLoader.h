#pragma once
#include "Transform.h"

#include <string>
#include <vector>

class SceneLoader
{
private: // 構造体定義
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
  bool LoadScene(const std::string& sceneFileName);

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