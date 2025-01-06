#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include <vector>
#include <memory>

class GameClearScene : public BaseScene {
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

	// sprite
	std::unique_ptr<Sprite> backGround_ = nullptr;
	std::unique_ptr<Sprite> titleText_ = nullptr;
	std::unique_ptr<Sprite> pressButtonText_ = nullptr;

	Vector2 backGround_Pos_ = {0.0f, 0.0f};
	Vector2 title_Pos_ = {396.0f, 170.0f};
	Vector2 pressButton_Pos_ = {530.8f, 463.2f};
};
