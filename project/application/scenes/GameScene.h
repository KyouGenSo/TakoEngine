#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include "Skydome.h"
#include "Ground.h"

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

	bool isDebug_ = false;

	// 天球
	std::unique_ptr<Skydome> skydome_ = nullptr;
	std::unique_ptr<Object3d> skydomeModel_ = nullptr;

	// 地面
	std::unique_ptr<Ground> ground_ = nullptr;
	std::unique_ptr<Object3d> groundModel_ = nullptr;
};
