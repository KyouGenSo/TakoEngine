#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include <vector>
#include <memory>
#include "Camera.h"
#include "ParticleEmitter.h"

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


private: // メンバ変数

	bool isDebug_ = false;

	Object3d* plane_;
	Vector3 planePos_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 planeScale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 planeRotate_ = Vector3(0.0f, 0.0f, 0.0f);

	Object3d* plane2_;
	Vector3 plane2Pos_ = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 plane2Scale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 plane2Rotate_ = Vector3(0.0f, 0.0f, 0.0f);

	Object3d* terrain_;
	Vector3 terrainPos_ = Vector3(0.0f, 1.0f, 0.0f);
	Vector3 terrainScale_ = Vector3(1.0f, 1.0f, 1.0f);
	Vector3 terrainRotate_ = Vector3(0.0f, 0.0f, 0.0f);

	Camera* camera_;
	Vector3 cameraPos_ = Vector3(0.0f, 9.0f, -34.0f);
	Vector3 cameraRot_ = Vector3(0.2f, 0.0f, 0.0f);
};
