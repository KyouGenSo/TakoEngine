#pragma once
#include "BaseScene.h"
#include "SkyBox.h"
#include "BoneTracker.h"
#include "SceneLoader.h"

class GameScene : public BaseScene
{
public: // メンバ関数
  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize() override;

  /// <summary>
  /// 終了処理
  /// </summary>
  void Finalize() override;

  /// <summary>
  /// 更新
  /// </summary>
  void Update() override;

  /// <summary>
  /// 描画
  /// </summary>
  void Draw() override;
  void DrawWithoutEffect() override;

  /// <summary>
  /// ImGuiの描画
  /// </summary>
  void DrawImGui() override;

private: // メンバ変数

  // SkyBox
  std::unique_ptr<SkyBox> skyBox_;

  bool isDebug_ = false;

  // SceneLoader関連
  std::unique_ptr<SceneLoader> sceneLoader_;
  std::unique_ptr<SceneLoader::LoadedScene> loadedScene_;
};
