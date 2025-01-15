#pragma once
#include "BaseScene.h"
#include"Sprite.h"
#include"Object3d.h"
#include <vector>
#include <memory>
#include "QuatFunc.h"

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

	Quaternion q1;
	Quaternion q2;
	Quaternion identity;
	Quaternion conj;
	Quaternion inv;
	Quaternion normal;
	Quaternion mul1;
	Quaternion mul2;

	float norm;
};
