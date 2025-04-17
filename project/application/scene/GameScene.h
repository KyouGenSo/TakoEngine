#pragma once
#include "BaseScene.h"
#include "EmitterManager.h"

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

  /// <summary>
  /// ImGuiの描画
  /// </summary>
  void DrawImGui() override;

private: // メンバ変数

  bool isDebug_ = false;

    // エミッター管理
  std::unique_ptr<EmitterManager> emitterManager_;

  // エミッター設定
  SphereEmitterParams spEmitterSett_ = {};
  BoxEmitterParams boxEmitterSett_ = {};
  TriangleEmitterParams triEmitterSett_ = {};
  Vector3 groupPosition_ = {};
  bool isActive_ = true;
};
