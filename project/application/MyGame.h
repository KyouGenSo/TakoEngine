#pragma once
#include"TakoFramework.h"
#include"Input.h"
#include"GameScene.h"

class MyGame : public TakoFramework
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

private: // メンバ変数

	// 入力クラス
	Input* input_ = nullptr;

	// ゲームシーン
	GameScene* gameScene_ = nullptr;

};