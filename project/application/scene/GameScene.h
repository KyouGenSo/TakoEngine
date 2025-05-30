#pragma once
#include "BaseScene.h"
#include"Object3d.h"
#include "EmitterManager.h"
#include "Sprite.h"
#include "SkyBox.h"

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

  std::unique_ptr <Object3d> object3d_ = nullptr;

  bool isDebug_ = false;

  // モデルの設定
  Vector3 modelScale_ = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  Vector3 modelPos_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  Vector3 modelRotate_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };

  // エミッター管理
  std::unique_ptr<EmitterManager> emitterManager_;

  // エミッター設定
  SphereEmitterParams hitEffect1Sett_ = {};
  SphereEmitterParams hitEffect2Sett_ = {};

  bool isHitEffect2_ = false;
  bool isRGBSplt_ = false;
  bool isRadialBlur_ = false;
  bool isBWFilter_ = false;

  Vector2 radialBlurCenter{ 0.5f, 0.5f };
  float radialBlurWidth = 0.0f;
  int32_t radialBlurSampleCount = 5;
  float radialBlurWidthSpeed = 0.001f;
  float radialBlurWidthMax = 0.1f;
  float radialBlurDuration = 0.5f;

  float bwFilterThreshold = 0.5f;
  // 白黒フィルターの継続時間
  float bwFilterDuration = 0.15f;

  float rgbSplitIntensity = 0.f;
  Vector2 redOffset = { 0.01f, 0.0f };
  Vector2 greenOffset = { -0.01f, 0.0f };
  Vector2 blueOffset = { 0.0f, 0.0f };
  float rgbSplitIntensitySpeed = 0.02f;
  float rgbSplitIntensityMax = 0.5f;

};
