#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include <vector>
#include <memory>
#include "Camera.h"
#include "ParticleEmitter.h"
#include "DirectXMath.h"

class TitleScene : public BaseScene
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

	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

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

private: // メンバ変数

	bool isDebug_ = false;

	bool enableLighting_ = true;

	Object3d* ball_;
	Vector3 ballPos_ = Vector3(5.3f, 2.f, -11.8f);
	Vector3 ballScale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 ballRotate_ = Vector3(0.0f, DirectX::XMConvertToRadians(-90.0f), 0.0f);
	bool ballHighlight_ = true;

	Object3d* plane_;
	Vector3 planePos_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 planeScale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 planeRotate_ = Vector3(0.0f, 0.0f, 0.0f);
	bool planeHighlight_ = true;

	Object3d* plane2_;
	Vector3 plane2Pos_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 plane2Scale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 plane2Rotate_ = Vector3(0.0f, 0.0f, 0.0f);
	bool plane2Highlight_ = true;

	Object3d* terrain_;
	Vector3 terrainPos_ = Vector3(0.0f, 1.0f, 0.0f);
	Vector3 terrainScale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 terrainRotate_ = Vector3(0.0f, 0.0f, 0.0f);
	bool terrainHighlight_ = false;

	Object3d* anim_Node_00_;
	Transform gltfAsset_Transform_;
	bool anim_Node_00_Highlight_ = false;

	Camera* camera_;
	Vector3 cameraPos_ = Vector3(0.0f, 9.0f, -34.0f);
	Vector3 cameraRot_ = Vector3(0.2f, 0.0f, 0.0f);

	// 平行光源の設定
	DirectionalLight directionalLightData_;

	// 点光源の設定
	PointLight pointLight_;
	PointLight pointLight2_;

	// スポットライトの設定
	SpotLight spotLight_;
	SpotLight spotLight2_;
};
