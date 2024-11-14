#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"

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

	Object3d* object3d_ = nullptr;
	Object3d* object3d2_ = nullptr;


	bool isDebug_ = false;

	// モデルの設定
	Vector3 modelScale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 modelPos_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 modelRotate_ = Vector3(0.0f, 0.0f, 0.0f);

	// ライトの設定
	float shininess_ = 100.0f;
	bool isLighting_ = true;
	bool isHighlight_ = true;
	Vector4 lightColor_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	Vector3 lightDirection_ = Vector3(0.0f, -1.0f, 0.0f);
	float lightIntensity_ = 0.0f;

	Vector3 pointLightPos_ = Vector3(0.0f, -5.37f, 22.0f);
	Vector4 pointLightColor_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	float pointLightIntensity_ = 1.0f;
	float pointLightRadius_ = 10.0f;
	float pointLightDecay_ = 1.0f;
	bool isPointLightEnable_ = false;

	// スポットライトの設定
	SpotLight spotLight_;
};
