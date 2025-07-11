#pragma once
#include "BaseScene.h"
#include"Object3d.h"
#include "EmitterManager.h"
#include "Sprite.h"
#include "SkyBox.h"
#include "BoneTracker.h"
#include "FollowCamera.h"

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

  void CharacterMove();

private: // メンバ変数

  // SkyBox
  std::unique_ptr<SkyBox> skyBox_;

  Object3d* object3d_ = nullptr;
  Object3d* characterModel_ = nullptr;

  std::unique_ptr<FollowCamera> followCamera_;

  bool isDebug_ = false;

  uint32_t SrvAllocateCount_ = 0;

  // モデルの設定
  Vector4 modelColor_ = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };

  Transform object3dTransform_{};
  Transform characterTransform_{};

  // 平行光源の設定
  float shininess_ = 100.0f;
  float envMapCoefficient_ = 1.0f;
  bool isLighting_ = true;
  bool isHighlight_ = true;
  bool enableEnvMap = true;
  Vector4 lightColor_ = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };
  Vector3 lightDirection_ = { .x = 0.0f, .y = -1.0f, .z = 0.0f };
  float lightIntensity_ = 1.0f;

    // エミッター管理
  std::unique_ptr<EmitterManager> emitterManager_;

  // ボーントラッカー
  std::unique_ptr<BoneTracker> boneTracker_;
};
