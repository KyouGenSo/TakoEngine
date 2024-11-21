#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include <vector>
#include"Vector2.h"

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

	Vector3 center;

	Vector3 cameraPosition = { 0.0f, 1.9f, -6.49f};
	Vector3 cameraRotation = { 0.26f, 0.0f, 0.0f };

	float windowX = 1280.0f;
	float windowY = 720.0f;
};
