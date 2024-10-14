#pragma once
#include"Audio.h"
#include"Sprite.h"
#include"Object3d.h"

class GameScene
{
public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// ImGuiの描画
	/// </summary>
	void DrawImGui();

private: // メンバ変数
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
