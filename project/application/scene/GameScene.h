#pragma once
#include "BaseScene.h"
#include"Object3d.h"
#include "EmitterManager.h"
#include "Sprite.h"
#include "SkyBox.h"
#include "BoneTracker.h"
#include "CharacterEffectPresets.h"

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

  Object3d* characterModel_ = nullptr;

  bool isDebug_ = false;

  // モデルのトランスフォーム
  Transform characterTransform_{};
  Transform characterTransform2_{};

  // 平行光源の設定
  float shininess_ = 100.0f;
  float envMapCoefficient_ = 1.0f;
  bool isLighting_ = true;
  bool isHighlight_ = true;
  bool enableEnvMap = true;

    // エミッター管理
  std::unique_ptr<EmitterManager> emitterManager_;
  EmitterManager::SphereEmitterSettings sphereEmitterSett_{};
  EmitterManager::BoxEmitterSettings boxEmitterSett_{};
  EmitterManager::TriangleEmitterSettings triangleEmitterSett_{};

};
