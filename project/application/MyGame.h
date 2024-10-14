#pragma once
#include"TakoFramework.h"
#include"Input.h"
#include"Audio.h"
#include"Sprite.h"
#include"Object3d.h"

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

	// オーディオクラス
	Audio* audio_ = nullptr;

	float volume = 1.0f;
	bool loopFlag = false;

	// スプライトの数
	uint32_t spriteNum_ = 2;

	// スプライト
	std::vector<Sprite*> sprites_;

	Object3d* object3d_;
	Object3d* object3d2_;

	uint32_t soundDataHandle;
	uint32_t voiceHandle;

	uint32_t bgmSH;
	uint32_t bgmVH;
};