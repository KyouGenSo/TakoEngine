#pragma once

#ifdef _DEBUG

#include <array>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <memory>
#include "PrimitiveEditor.h"
#include "ParticleEditor.h"
#include "DebugViewport.h"

namespace Tako {

  /// <summary>
  /// デバッグ UI の統合管理クラス。シーンヒエラルキー、インスペクター、コンソール、パフォーマンスモニターなどを提供
  /// </summary>
  class DebugUIManager {
  public: //構造体
    /// <summary>
    /// ログタイプ
    /// </summary>
    enum class LogType {
      Info,
      Warning,
      Error
    };

    /// <summary>
    /// デバッグ UI のウィンドウ種別
    /// </summary>
    enum class Window : int {
      GameViewport,
      DebugViewport,
      SceneHierarchy,
      Inspector,
      Console,
      Performance,
      EngineStatus,
      InputDebug,
      ShadowSettings,
      CollisionDebug,
      PostEffect,
      ParticleEditor,
      GlobalVariables,
      PrimitiveEditor,
      Count
    };

    /// <summary>
    /// ログ構造体
    /// </summary>
    struct LogEntry {
      LogType type;
      std::string message;
      std::string timestamp;
    };

    /// <summary>
    /// ゲームオブジェクトデバッグ情報
    /// </summary>
    struct GameObjectDebugInfo {
      std::string           name;           ///< オブジェクト名
      std::function<void()> drawImGuiFunc;  ///< DrawImGui 関数
    };

  private:
    static std::unique_ptr<DebugUIManager> instance_;  ///< シングルトン
    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~DebugUIManager();

    friend struct std::default_delete<DebugUIManager>;

  public:
    explicit DebugUIManager(Token);
    DebugUIManager(const DebugUIManager&) = delete;
    DebugUIManager& operator=(const DebugUIManager&) = delete;

  public: //メンバー関数
    /// <summary>
    /// インスタンス取得
    /// </summary>
    /// <returns>DebugUIManager のインスタンス</returns>
    static DebugUIManager* GetInstance();

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
    /// プリミティブエディターのプレビューをオフスクリーン RT へ描画
    /// TakoFramework::Draw() のシーン描画後・ImGui 描画前に呼ぶ
    /// </summary>
    void DrawPrimitivePreviewPass() { primitiveEditor_.DrawPreviewPass(); }

    /// <summary>
    /// パーティクルエディターのプレビューをオフスクリーン RT へ描画
    /// TakoFramework::Draw() の GPUParticle::Draw() 後・ImGui 描画前に呼ぶ
    /// </summary>
    void DrawParticlePreviewPass() { particleEditor_.DrawPreviewPass(); }

    /// <summary>
    /// デバッグビューポートへシーンをデバッグカメラ視点で描画
    /// TakoFramework::Draw() の本編描画後・LineRenderer::Reset() 前に呼ぶ（本編の線分を流用するため）
    /// </summary>
    void DrawDebugViewportPass() { debugViewport_.DrawPass(); }

    /// <summary>
    /// コンソールにログを追加
    /// </summary>
    /// <param name="message">ログメッセージ</param>
    /// <param name="type">ログの種類</param>
    void AddLog(const std::string& message, LogType type = LogType::Info);

    /// <summary>
    /// ログをクリア
    /// </summary>
    void ClearLogs();

    /// <summary>
    /// ゲームオブジェクトを登録
    /// </summary>
    /// <param name="name">オブジェクト名</param>
    /// <param name="drawImGuiFunc">ImGui 描画関数</param>
    void RegisterGameObject(const std::string& name, std::function<void()> drawImGuiFunc);

    /// <summary>
    /// ゲームオブジェクトの登録を解除
    /// </summary>
    /// <param name="name">オブジェクト名</param>
    void UnregisterGameObject(const std::string& name);

    /// <summary>
    /// ゲームオブジェクトをクリア
    /// </summary>
    void ClearGameObjects();

    //============================================================
    //Setter
    //============================================================
    void SetWindowVisible(Window window, bool visible) { WindowFlag(window) = visible; }
    void SetSceneName(const std::string& sceneName) { currentSceneName_ = sceneName; }
    void SetEmitterManager(EmitterManager* emitterManager) { particleEditor_.SetEmitterManager(emitterManager); }
    void SetForceFieldManager(ForceFieldManager* forceFieldManager) { particleEditor_.SetForceFieldManager(forceFieldManager); }
    void SetEndFlagPtr(bool* pEndFlag) { pEndFlag_ = pEndFlag; }

    //============================================================
    //Getter
    //============================================================
    const std::vector<LogEntry>& GetLogs() const { return consoleLogs_; }

    bool IsWindowVisible(Window window) const { return windowVisibility_[static_cast<size_t>(window)]; }

    /// <summary>
    /// カーソルがゲーム描画領域上にあるか。
    /// GameViewport 表示中はゲーム画像上、非表示(フルスクリーン)中は
    /// いずれの ImGui ウィンドウにもカーソルが無い状態を指す。
    /// ゲーム入力を ImGui 操作と排他にするゲートとして使う。
    /// </summary>
    /// <returns>カーソルがゲーム描画領域上にある場合 true</returns>
    bool IsCursorOverGameView() const;

    const std::string& GetSceneName() const { return currentSceneName_; }

  private: //非公開関数
    /// <summary>
    /// ウィンドウ表示フラグへの参照（ImGui の p_open やエディターへ渡すアドレスとしても使う）
    /// </summary>
    /// <param name="window">ウィンドウ種別</param>
    /// <returns>表示フラグ</returns>
    bool& WindowFlag(Window window) { return windowVisibility_[static_cast<size_t>(window)]; }

    /// <summary>
    /// ウィンドウの表示状態を反転
    /// </summary>
    /// <param name="window">ウィンドウ種別</param>
    void ToggleWindow(Window window) { WindowFlag(window) = !WindowFlag(window); }

    /// <summary>
    /// メインメニューバーを描画
    /// </summary>
    void DrawMainMenuBar();

    /// <summary>
    /// シーンヒエラルキーウィンドウを描画
    /// </summary>
    void DrawSceneHierarchy();

    /// <summary>
    /// インスペクターウィンドウを描画
    /// </summary>
    void DrawInspector();

    /// <summary>
    /// コンソールウィンドウを描画
    /// </summary>
    void DrawConsole();

    /// <summary>
    /// パフォーマンスウィンドウを描画
    /// </summary>
    void DrawPerformance();

    /// <summary>
    /// ゲームビューポートを描画
    /// </summary>
    void DrawGameViewport();

    /// <summary>
    /// エンジンステータスウィンドウを描画
    /// </summary>
    void DrawEngineStatus();

    /// <summary>
    /// 入力デバッグウィンドウを描画
    /// </summary>
    void DrawInputDebug();

    /// <summary>
    /// シャドウ設定ウィンドウを描画
    /// </summary>
    void DrawShadowSettings();

    /// <summary>
    /// コリジョンデバッグウィンドウを描画
    /// </summary>
    void DrawCollisionDebug();

    /// <summary>
    /// 現在のタイムスタンプを生成
    /// </summary>
    /// <returns>タイムスタンプ文字列</returns>
    std::string GetCurrentTimestamp();

  private: //メンバー変数

    //コンソールログ
    std::vector<LogEntry> consoleLogs_;
    int                   maxConsoleLogs_ = 1000;
    bool                  showInfo_       = true;
    bool                  showWarning_    = true;
    bool                  showError_      = true;
    bool                  autoScroll_     = true;

    std::array<bool, static_cast<size_t>(Window::Count)> windowVisibility_{};  ///< ウィンドウ表示フラグ（Window で添字）

    bool isGameViewportHovered_  = false;  ///< 直近フレームでゲーム画像上にカーソルがあったか（DrawGameViewport で更新）
    bool isPreviewInputCaptured_ = false;  ///< エディタプレビュー/デバッグビューがホバー中/操作中か。true の間は次フレームのゲーム入力を遮断する

    //ゲームオブジェクト情報
    std::vector<GameObjectDebugInfo> gameObjects_;
    int                              selectedObjectIndex_ = -1;  ///< 選択されたオブジェクトのインデックス

    //パフォーマンス計測
    float fpsHistory_[100] = { 0 };
    int   fpsHistoryIndex_ = 0;

    std::string currentSceneName_ = "Unknown";  ///< 現在のシーン名

    bool* pEndFlag_ = nullptr;  ///< アプリケーション終了フラグへのポインタ

    //シーン遷移 UI 用
    char sceneNameBuffer_[128] = "";  ///< シーン名入力バッファ

    //ツール
    PrimitiveEditor primitiveEditor_;
    ParticleEditor  particleEditor_;
    DebugViewport   debugViewport_;
  };

} // namespace Tako

#endif // _DEBUG