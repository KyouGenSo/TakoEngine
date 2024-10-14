#pragma once
#include"WinApp.h"
#include"DX12Basic.h"
#include"Input.h"
#include"Audio.h"
#include"SpriteBasic.h"
#include"Sprite.h"
#include"D3DResourceLeakCheker.h"
#include"TextureManager.h"
#include"Object3d.h"
#include"Object3dBasic.h"
#include"Model.h"
#include"ModelManager.h"
#include"Camera.h"
#include"SrvManager.h"

#ifdef _DEBUG
#include"ImGuiManager.h"
#endif

class MyGame
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

private: // メンバ変数
	// リソースリークチェッカー
	D3DResourceLeakCheker d3dResourceLeakCheker;

	// ウィンドウクラス
	WinApp* winApp_ = nullptr;

	// DX12
	DX12Basic* dx12_ = nullptr;

	// ImGuiManager
#ifdef _DEBUG
	ImGuiManager* imguiManager_ = nullptr;
#endif

	// SRVマネージャー
	SrvManager* srvManager_ = nullptr;

	// オブジェクト3D基本クラス
	Object3dBasic* object3dBasic_ = nullptr;

	// スプライト共通クラス
	SpriteBasic* spriteBasic_ = nullptr;

	// カメラ
	Camera* defaultCamera_ = nullptr;

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