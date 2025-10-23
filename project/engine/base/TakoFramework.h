#pragma once
#include "WinApp.h"
#include "DX12Basic.h"
#include "D3DResourceLeakChecker.h"
#include "Camera.h"
#include "SrvManager.h"
#include "AbstractSceneFactory.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#include "DebugUIManager.h"
#endif

/// <summary>
/// ゲームエンジンのメインフレームワーク
/// アプリケーションの基底クラス
/// </summary>
class TakoFramework {
public: // メンバ関数

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~TakoFramework() = default;

	/// <summary>
	/// アプリケーションの初期化
	/// </summary>
	virtual void Initialize();

	/// <summary>
	/// 終了処理
	/// </summary>
	virtual void Finalize();

	/// <summary>
	/// フレーム更新処理
	/// </summary>
	virtual void Update();

	/// <summary>
	/// 描画処理
	/// </summary>
	virtual void Draw();

	/// <summary>
	/// アプリケーションのメインループ実行
	/// </summary>
	void Run();

	/// <summary>
	/// 終了フラグを取得
	/// </summary>
	/// <returns>終了フラグ</returns>
	bool GetEndFlag() const { return endFlag_; }

#ifdef _DEBUG
	/// <summary>
	/// デバッグモードの取得
	/// </summary>
	/// <returns>デバッグモードフラグ</returns>
	bool GetIsDebug() const { return isDebug_; }

	/// <summary>
	/// デバッグモードの設定
	/// </summary>
	/// <param name="value">デバッグモードフラグ</param>
	void SetIsDebug(bool value);

	/// <summary>
	/// デバッグフラグのポインタ取得（ImGui用）
	/// </summary>
	/// <returns>デバッグフラグへのポインタ</returns>
	bool* GetIsDebugPtr() { return &isDebug_; }
#endif

	/// <summary>
	/// フルスクリーンモードの切り替え
	/// </summary>
	void ToggleFullScreen();

	/// <summary>
	/// ウィンドウリサイズ時の処理
	/// </summary>
	/// <param name="width">新しいウィンドウ幅</param>
	/// <param name="height">新しいウィンドウ高さ</param>
	void OnWindowResize(uint32_t width, uint32_t height);

protected: // メンバ変数
	D3DResourceLeakChecker d3dResourceLeakCheker;  ///< リソースリークチェッカー（デバッグビルドでメモリリーク検出）

	WinApp* winApp_ = nullptr;  ///< ウィンドウ管理クラスへのポインタ

	DX12Basic* dx12_ = nullptr;  ///< DirectX 12基盤システムへのポインタ

#ifdef _DEBUG
	ImGuiManager* imguiManager_ = nullptr;  ///< ImGuiマネージャー（デバッグUI用）
#endif

	Camera* defaultCamera_ = nullptr;  ///< デフォルトカメラ

	AbstractSceneFactory* sceneFactory_ = nullptr;  ///< シーンファクトリー（シーン生成用）

	bool endFlag_ = false;  ///< アプリケーション終了フラグ

	bool isDebug_ = false;  ///< デバッグモードフラグ
};