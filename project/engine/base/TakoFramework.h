#pragma once
#include "WinApp.h"
#include "DX12Basic.h"
#include "D3DResourceLeakCheker.h"
#include "Camera.h"
#include "SrvManager.h"
#include "AbstractSceneFactory.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

class TakoFramework {
public: // メンバ関数

	// デストラクタ
	virtual ~TakoFramework() = default;

	// 初期化
	virtual void Initialize();

	// 終了処理
	virtual void Finalize();

	// 更新
	virtual void Update();

	// 描画
	virtual void Draw() = 0;

	// 実行
	void Run();

	// 終了フラグを取得
	bool GetEndFlag() const { return endFlag_; }

protected: // メンバ変数
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

	// カメラ
	Camera* defaultCamera_ = nullptr;

	// シーンファクトリー
	AbstractSceneFactory* sceneFactory_ = nullptr;

	// 終了フラグ
	bool endFlag_ = false;
};