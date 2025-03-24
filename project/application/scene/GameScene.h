#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include "GPUParticleEmitter.h"
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

  // sphereEmitterの設定
  struct SphereEmitterSetting
  {
    Vector3 position;
    float radius;
    uint32_t count;
    float frequency;
  };

  // boxEmitterの設定
  struct BoxEmitterSetting
  {
    Vector3 position;
    Vector3 size;
    Vector3 rotation;
    uint32_t count;
    float frequency;
  };

  // triangleEmitterの設定
  struct TriangleEmitterSetting
  {
    Vector3 position;
    Vector3 v1;
    Vector3 v2;
    Vector3 v3;
    uint32_t count;
    float frequency;
  };

	Object3d* object3d_ = nullptr;
	Object3d* object3d2_ = nullptr;

	bool isDebug_ = false;

	// モデルの設定
  Vector3 modelScale_ = { 1.0f, 1.0f, 1.0f };
  Vector3 modelPos_ = { 0.0f, 0.0f, 0.0f };
  Vector3 modelRotate_ = { 0.0f, 0.0f, 0.0f };

  Vector3 modelScale2_ = { 1.0f, 1.0f, 1.0f };
  Vector3 modelPos2_ = { 0.0f, 0.0f, 0.0f };
  Vector3 modelRotate2_ = { 0.0f, 0.0f, 0.0f };

	// 平行光源の設定
	float shininess_ = 100.0f;
	bool isLighting_ = true;
	bool isHighlight_ = true;
  Vector4 lightColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
  Vector3 lightDirection_ = { 0.0f, -1.0f, 0.0f };
	float lightIntensity_ = 0.5f;

  // マテリアルの設定
  Vector4 materialColor1_ = { 1.0f, 1.0f, 1.0f, 1.0f };
  Vector4 materialColor2_ = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 点光源の設定
	PointLight pointLight_;
	PointLight pointLight2_;

	// スポットライトの設定
	SpotLight spotLight_;

  // エミッター管理
  std::unique_ptr<EmitterManager> emitterManager_;

  // 球体エミッター
  std::shared_ptr<SphereEmitter> sphereEmitter_;

  // 箱型エミッター
  std::shared_ptr<BoxEmitter> boxEmitter_;

  // 三角形エミッター
  std::shared_ptr<TriangleEmitter> triangleEmitter_;

  // エミッター設定
  SphereEmitterSetting spEmitterSett_;
  BoxEmitterSetting boxEmitterSett_;
  TriangleEmitterSetting triEmitterSett_;
};
