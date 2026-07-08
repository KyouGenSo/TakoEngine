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
    DebugUIManager() = default;
    ~DebugUIManager() = default;

    friend struct std::default_delete<DebugUIManager>;

  public:
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
    void SetWindowVisible(const std::string& windowName, bool visible);
    void SetSceneName(const std::string& sceneName) { currentSceneName_ = sceneName; }
    void SetEmitterManager(class EmitterManager* emitterManager) { emitterManager_ = emitterManager; }
    void SetForceFieldManager(class ForceFieldManager* forceFieldManager) { forceFieldManager_ = forceFieldManager; }
    void SetEndFlagPtr(bool* pEndFlag) { pEndFlag_ = pEndFlag; }
    void SetDebugFlagPtr(bool* pIsDebug) { pIsDebug_ = pIsDebug; }

    //============================================================
    //Getter
    //============================================================
    const std::vector<LogEntry>& GetLogs() const { return consoleLogs_; }

    /// <summary>
    /// ウィンドウの表示状態を取得
    /// </summary>
    /// <param name="windowName">ウィンドウ名</param>
    /// <returns>表示状態</returns>
    bool IsWindowVisible(const std::string& windowName) const;

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

  private: //メンバー変数

    //コンソールログ
    std::vector<LogEntry> consoleLogs_;
    int                   maxConsoleLogs_ = 1000;
    bool                  showInfo_       = true;
    bool                  showWarning_    = true;
    bool                  showError_      = true;
    bool                  autoScroll_     = true;

    std::unordered_map<std::string, bool> windowVisibility_;  ///< ウィンドウ表示フラグ

    bool isGameViewportHovered_ = false;  ///< 直近フレームでゲーム画像上にカーソルがあったか（DrawGameViewport で更新）

    //ゲームオブジェクト情報
    std::vector<GameObjectDebugInfo> gameObjects_;
    int                              selectedObjectIndex_ = -1;  ///< 選択されたオブジェクトのインデックス

    //パフォーマンス計測
    float fpsHistory_[100] = { 0 };
    int   fpsHistoryIndex_ = 0;

    std::string currentSceneName_ = "Unknown";  ///< 現在のシーン名

    bool* pEndFlag_ = nullptr;  ///< アプリケーション終了フラグへのポインタ

    bool* pIsDebug_ = nullptr;  ///< デバッグカメラ有効フラグへのポインタ

    //シーン遷移 UI 用
    char sceneNameBuffer_[128] = "";  ///< シーン名入力バッファ

    //パーティクルエディター用
    class EmitterManager* emitterManager_            = nullptr;
    int                   selectedEmitterIndex_      = -1;
    char                  newEmitterNameBuffer_[128] = "";
    char                  presetNameBuffer_[128]     = "";
    char                  loadPresetBuffer_[128]     = "";
    bool                  showPresetManager_         = false;

    //グループ管理用
    int  selectedGroupIndex_      = -1;
    char newGroupNameBuffer_[128] = "";

    //フォースフィールド管理用
    class ForceFieldManager* forceFieldManager_       = nullptr;
    int                      selectedForceFieldIndex_ = -1;
    char                     ffPresetSaveBuffer_[128] = "";
    char                     ffPresetLoadBuffer_[128] = "";
    std::string              currentFFPresetName_;  ///< 最後にロード/保存したFFプリセット名（上書き保存用）

    //パーティクル可視化設定
    bool    showEmitterShapes_        = false;                       ///< エミッター形状の表示ON/OFF
    bool    showForceFieldRadius_     = false;                       ///< フォースフィールド影響半径の表示ON/OFF
    bool    showForceFieldDirection_  = false;                       ///< フォースフィールド方向表示ON/OFF
    Vector4 emitterColorSphere_       = { 0.0f, 1.0f, 0.0f, 1.0f };  ///< 球エミッター色（緑）
    Vector4 emitterColorBox_          = { 0.0f, 0.5f, 1.0f, 1.0f };  ///< 箱エミッター色（青）
    Vector4 emitterColorTriangle_     = { 1.0f, 1.0f, 0.0f, 1.0f };  ///< 三角形エミッター色（黄）
    Vector4 forceFieldRadiusColor_    = { 1.0f, 0.5f, 0.0f, 0.5f };  ///< フォースフィールド半径色（オレンジ）
    Vector4 forceFieldDirectionColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };  ///< フォースフィールド方向色（赤）
    float   forceFieldArrowLength_    = 2.0f;                        ///< フォースフィールド矢印の長さ
    float   forceFieldArrowHeadSize_  = 0.3f;                        ///< フォースフィールド矢印の先端サイズ
  };

} // namespace Tako

#endif // _DEBUG