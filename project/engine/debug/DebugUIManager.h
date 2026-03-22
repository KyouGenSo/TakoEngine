#pragma once

#ifdef _DEBUG

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <memory>
#include "Vector3.h"
#include "Vector4.h"

namespace Tako {

  /// <summary>
  /// デバッグ UI の統合管理クラス。シーンヒエラルキー、インスペクター、コンソール、パフォーマンスモニターなどを提供
  /// </summary>
  class DebugUIManager {
  public:
    // ログタイプ
    enum class LogType {
      Info,
      Warning,
      Error
    };

    // ログ構造体
    struct LogEntry {
      LogType type;
      std::string message;
      std::string timestamp;
    };

    // ゲームオブジェクトデバッグ情報
    struct GameObjectDebugInfo {
      std::string name;                    // オブジェクト名
      std::function<void()> drawImGuiFunc; // DrawImGui 関数
    };

  private:
    // シングルトン
    static std::unique_ptr<DebugUIManager> instance_;
    DebugUIManager() = default;
    ~DebugUIManager() = default;

    friend struct std::default_delete<DebugUIManager>;

  public:
    DebugUIManager(const DebugUIManager&) = delete;
    DebugUIManager& operator=(const DebugUIManager&) = delete;

  private:

  public:
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
    /// ログリストを取得
    /// </summary>
    /// <returns>ログエントリーのリスト</returns>
    const std::vector<LogEntry>& GetLogs() const { return consoleLogs_; }

    /// <summary>
    /// デバッグ情報を登録
    /// </summary>
    /// <param name="category">カテゴリ名</param>
    /// <param name="callback">デバッグ表示用コールバック</param>
    void RegisterDebugInfo(const std::string& category, std::function<void()> callback);

    /// <summary>
    /// デバッグ情報の登録を解除
    /// </summary>
    /// <param name="category">カテゴリ名</param>
    void UnregisterDebugInfo(const std::string& category);

    /// <summary>
    /// デバッグ情報をクリア
    /// </summary>
    void ClearDebugInfo();

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

    /// <summary>
    /// ウィンドウの表示状態を設定
    /// </summary>
    /// <param name="windowName">ウィンドウ名</param>
    /// <param name="visible">表示フラグ</param>
    void SetWindowVisible(const std::string& windowName, bool visible);

    /// <summary>
    /// ウィンドウの表示状態を取得
    /// </summary>
    /// <param name="windowName">ウィンドウ名</param>
    /// <returns>表示状態</returns>
    bool IsWindowVisible(const std::string& windowName) const;

    /// <summary>
    /// シーン名を設定
    /// </summary>
    /// <param name="sceneName">シーン名</param>
    void SetSceneName(const std::string& sceneName) { currentSceneName_ = sceneName; }

    /// <summary>
    /// シーン名を取得
    /// </summary>
    /// <returns>現在のシーン名</returns>
    const std::string& GetSceneName() const { return currentSceneName_; }

    /// <summary>
    /// EmitterManager を設定
    /// </summary>
    /// <param name="emitterManager">EmitterManager ポインタ</param>
    void SetEmitterManager(class EmitterManager* emitterManager) { emitterManager_ = emitterManager; }

  private:
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
    /// パーティクルエディターを描画
    /// </summary>
    void DrawParticleEditor();

    /// <summary>
    /// グループ管理タブを描画
    /// </summary>
    void DrawGroupsTab();

    /// <summary>
    /// フォースフィールド管理タブを描画
    /// </summary>
    void DrawForceFieldsTab();

    /// <summary>
    /// パーティクル可視化の描画（エミッター形状 + フォースフィールド）
    /// </summary>
    void DrawParticleVisualization();

    /// <summary>
    /// パーティクル可視化設定UIの描画（Visualizationタブ内）
    /// </summary>
    void DrawVisualizationTab();

    /// <summary>
    /// 個別エミッターの形状を描画
    /// </summary>
    /// <param name="emitter">描画対象のエミッター</param>
    void DrawEmitterShape(const std::shared_ptr<class GPUParticleEmitter>& emitter);

    /// <summary>
    /// 個別フォースフィールドの可視化を描画
    /// </summary>
    /// <param name="field">描画対象のフォースフィールド</param>
    /// <param name="index">フィールドのインデックス（ハイライト判定用）</param>
    void DrawForceFieldVisualization(const struct ForceFieldData& field, int index);

    /// <summary>
    /// 現在のタイムスタンプを生成
    /// </summary>
    /// <returns>タイムスタンプ文字列</returns>
    std::string GetCurrentTimestamp();

  private:
    // コンソールログ
    std::vector<LogEntry> consoleLogs_;
    int maxConsoleLogs_ = 1000;
    bool showInfo_ = true;
    bool showWarning_ = true;
    bool showError_ = true;
    bool autoScroll_ = true;

    // ウィンドウ表示フラグ
    std::unordered_map<std::string, bool> windowVisibility_;

    // デバッグ情報コールバック
    std::unordered_map<std::string, std::function<void()>> debugInfoCallbacks_;

    // ゲームオブジェクト情報
    std::vector<GameObjectDebugInfo> gameObjects_;
    int selectedObjectIndex_ = -1;  // 選択されたオブジェクトのインデックス

    // パフォーマンス計測
    float fpsHistory_[100] = { 0 };
    int fpsHistoryIndex_ = 0;

    // 現在のシーン名
    std::string currentSceneName_ = "Unknown";

    // アプリケーション終了フラグへのポインタ
    bool* pEndFlag_ = nullptr;

  public:
    /// <summary>
    /// 終了フラグポインタを設定
    /// </summary>
    /// <param name="pEndFlag">終了フラグへのポインタ</param>
    void SetEndFlagPtr(bool* pEndFlag) { pEndFlag_ = pEndFlag; }

    /// <summary>
    /// デバッグフラグポインタを設定
    /// </summary>
    /// <param name="pIsDebug">デバッグフラグへのポインタ</param>
    void SetDebugFlagPtr(bool* pIsDebug) { pIsDebug_ = pIsDebug; }

  private:
    // デバッグカメラ有効フラグへのポインタ
    bool* pIsDebug_ = nullptr;

    // シーン遷移 UI 用
    char sceneNameBuffer_[128] = "";  // シーン名入力バッファ

    // パーティクルエディター用
    class EmitterManager* emitterManager_ = nullptr;
    int selectedEmitterIndex_ = -1;
    char newEmitterNameBuffer_[128] = "";
    char presetNameBuffer_[128] = "";
    char loadPresetBuffer_[128] = "";
    bool showPresetManager_ = false;

    // グループ管理用
    int selectedGroupIndex_ = -1;
    char newGroupNameBuffer_[128] = "";

    // フォースフィールド管理用
    int selectedForceFieldIndex_ = -1;

    // パーティクル可視化設定
    bool showEmitterShapes_ = true;        ///< エミッター形状の表示ON/OFF
    bool showForceFieldRadius_ = true;     ///< フォースフィールド影響半径の表示ON/OFF
    bool showForceFieldDirection_ = true;  ///< フォースフィールド方向表示ON/OFF
    Vector4 emitterColorSphere_   = { 0.0f, 1.0f, 0.0f, 1.0f }; ///< 球エミッター色（緑）
    Vector4 emitterColorBox_      = { 0.0f, 0.5f, 1.0f, 1.0f }; ///< 箱エミッター色（青）
    Vector4 emitterColorTriangle_ = { 1.0f, 1.0f, 0.0f, 1.0f }; ///< 三角形エミッター色（黄）
    Vector4 forceFieldRadiusColor_    = { 1.0f, 0.5f, 0.0f, 0.5f }; ///< フォースフィールド半径色（オレンジ）
    Vector4 forceFieldDirectionColor_ = { 1.0f, 0.0f, 0.0f, 1.0f }; ///< フォースフィールド方向色（赤）
  };

} // namespace Tako

#endif // _DEBUG