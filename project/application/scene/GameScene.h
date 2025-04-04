#pragma once
#include "BaseScene.h"
#include"Object3d.h"
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

  // スポットライトデータ
  struct SpotLight
  {
    Vector4 color;
    Vector3 position;
    float intensity;
    Vector3 direction;
    float distance;
    float decay;
    float cosAngle;
    bool enable;
  };

  // 点光源データ
  struct PointLight
  {
    Vector4 color;
    Vector3 position;
    float intensity;
    float radius;
    float decay;
    bool enable;
  };

  Object3d* object3d_ = nullptr;
  Object3d* object3d2_ = nullptr;

  bool isDebug_ = false;

  // モデルの設定
  Vector3 modelScale_ = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  Vector3 modelPos_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  Vector3 modelRotate_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };

  Vector3 modelScale2_ = { .x = 1.0f, .y = 1.0f, .z = 1.0f };
  Vector3 modelPos2_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };
  Vector3 modelRotate2_ = { .x = 0.0f, .y = 0.0f, .z = 0.0f };

  // 平行光源の設定
  float shininess_ = 100.0f;
  bool isLighting_ = true;
  bool isHighlight_ = true;
  Vector4 lightColor_ = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };
  Vector3 lightDirection_ = { .x = 0.0f, .y = -1.0f, .z = 0.0f };
  float lightIntensity_ = 0.5f;

    // エミッター管理
  std::unique_ptr<EmitterManager> emitterManager_;

  // エミッター設定
  SphereEmitterParams spEmitterSett_ = {};
  BoxEmitterParams boxEmitterSett_ = {};
  TriangleEmitterParams triEmitterSett_ = {};
  Vector3 groupPosition_ = {};
  bool isActive_ = true;
};
