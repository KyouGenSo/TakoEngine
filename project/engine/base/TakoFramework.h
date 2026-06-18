#pragma once
#include "WinApp.h"
#include "DX12Basic.h"
#include "D3DResourceLeakChecker.h"
#include "Camera.h"
#include "AbstractSceneFactory.h"

#include <memory>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {


  /// <summary>
  /// ゲームエンジンのメインフレームワーク
  /// アプリケーションの基底クラス
  /// </summary>
  class TakoFramework {
  public: //メンバー関数

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
    /// フルスクリーンモードの切り替え
    /// </summary>
    void ToggleFullScreen();

    /// <summary>
    /// ウィンドウリサイズ時の処理
    /// </summary>
    /// <param name="width">新しいウィンドウ幅</param>
    /// <param name="height">新しいウィンドウ高さ</param>
    void OnWindowResize(uint32_t width, uint32_t height);

#ifdef _DEBUG
    bool GetIsDebug() const { return isDebug_; }

    /// <summary>
    /// デバッグモードの設定
    /// </summary>
    /// <param name="value">デバッグモードフラグ</param>
    void SetIsDebug(bool value);
#endif

    //===================================================
    //Getter
    //===================================================
    bool GetEndFlag() const { return endFlag_; }

  private: //非公開関数
    /// <summary>
    /// エンジンリソースの読み込み
    /// </summary>
    void LoadResources();

  protected: //メンバー変数
    D3DResourceLeakChecker d3dResourceLeakChecker;  ///< リソースリークチェッカー（デバッグビルドでメモリリーク検出）

    WinApp* winApp_ = nullptr;  ///< ウィンドウ管理クラスへのポインタ

    std::unique_ptr<DX12Basic> dx12_;  ///< DirectX 12基盤システムへのポインタ

#ifdef _DEBUG
    std::unique_ptr<ImGuiManager> imguiManager_;  ///< ImGui マネージャー（デバッグ UI 用）
#endif

    std::unique_ptr<Camera> defaultCamera_;  ///< デフォルトカメラ

    std::unique_ptr<AbstractSceneFactory> sceneFactory_;  ///< シーンファクトリー（シーン生成用）

    uint32_t spriteBasicOnresizeId_ = 0;

    bool endFlag_ = false;  ///< アプリケーション終了フラグ

    bool isDebug_ = false;  ///< デバッグモードフラグ
  };

} // namespace Tako