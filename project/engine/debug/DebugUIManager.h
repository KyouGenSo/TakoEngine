#pragma once

#ifdef _DEBUG

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <memory>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "PrimitiveBuilder.h"
#include "OrbitCameraController.h"

namespace Tako {

  class Object3d;
  class Camera;
  class PreviewViewport;

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

    /// <summary>
    /// プリミティブエディターの編集対象タイプ
    /// </summary>
    enum class PrimitiveType : int {
      Cube = 0,
      Sphere,
      Plane,
      Ring,
      Cylinder,
      Torus
    };

    /// <summary>
    /// パーティクルエディターのインスペクタ表示対象
    /// </summary>
    enum class ParticleInspectTarget : int {
      None = 0,
      Emitter,
      ForceField,
      Group
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
    void DrawPrimitivePreviewPass();

    /// <summary>
    /// パーティクルエディターのプレビューをオフスクリーン RT へ描画
    /// TakoFramework::Draw() の GPUParticle::Draw() 後・ImGui 描画前に呼ぶ
    /// </summary>
    void DrawParticlePreviewPass();

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
    /// パーティクルエディターを描画（3ペイン: リスト / プレビュー / インスペクタ）
    /// </summary>
    void DrawParticleEditor();

    /// <summary>
    /// パーティクルエディターの更新（プレビュー RT/カメラの生成、オービットカメラ反映）
    /// GPU アイドル区間である Update() から呼ぶこと
    /// </summary>
    void UpdateParticleEditor();

    /// <summary>
    /// パーティクルエディターの GPU リソースを解放
    /// </summary>
    void FinalizeParticleEditor();

    /// <summary>
    /// 左ペイン: エミッター / フォースフィールド / グループの各セクションを描画
    /// </summary>
    void DrawParticleEditorListPane();

    /// <summary>
    /// 左ペイン: エミッター作成フォーム / 一覧 / Delete・Duplicate / クリップボード
    /// </summary>
    void DrawParticleEditorEmitterSection();

    /// <summary>
    /// 左ペイン: フォースフィールド追加フォーム / 一覧
    /// </summary>
    void DrawParticleEditorForceFieldSection();

    /// <summary>
    /// 左ペイン: グループ作成 / 一覧
    /// </summary>
    void DrawParticleEditorGroupSection();

    /// <summary>
    /// 中央ペイン: プレビューツールバー + プレビュー画像 + オービットカメラ入力
    /// </summary>
    void DrawParticleEditorPreviewPane();

    /// <summary>
    /// 右ペイン: 選択対象別インスペクタ + 共通セクション（Scene Presets / Visualization）
    /// </summary>
    void DrawParticleEditorInspectorPane();

    /// <summary>
    /// 選択中エミッターの全プロパティ / リネーム / プリセット保存・読込を描画
    /// </summary>
    void DrawEmitterInspector();

    /// <summary>
    /// 選択中フォースフィールドの編集 / 削除 / FF プリセットを描画
    /// </summary>
    void DrawForceFieldInspector();

    /// <summary>
    /// 選択中グループの操作（Active / 位置 / 所属エミッター管理 / 削除）を描画
    /// </summary>
    void DrawGroupInspector();

    /// <summary>
    /// パーティクル可視化の描画（エミッター形状 + フォースフィールド）
    /// </summary>
    void DrawParticleVisualization();

    /// <summary>
    /// パーティクル可視化設定UIの描画（メインシーンへの線描画の ON/OFF と色）
    /// </summary>
    void DrawVisualizationSettings();

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
    /// プリミティブエディターウィンドウを描画（ビューポート + パラメータ）
    /// </summary>
    void DrawPrimitiveEditor();

    /// <summary>
    /// プリミティブエディターの更新（プレビュー生成/破棄、dirty 時のモデル再生成、カメラ更新）
    /// GPU アイドル区間である Update() から呼ぶこと（描画フェーズでのモデル差し替えは危険）
    /// </summary>
    void UpdatePrimitiveEditor();

    /// <summary>
    /// プリミティブエディターの GPU リソースを解放
    /// </summary>
    void FinalizePrimitiveEditor();

    /// <summary>
    /// 現在のパラメータでプレビューモデルを再生成して差し替える
    /// </summary>
    void RebuildPrimitivePreview();

    /// <summary>
    /// 色/ライティング/半透明をプレビューへ再適用（Model 差し替えで消えるため毎フレーム）
    /// </summary>
    void ApplyPrimitivePreviewSettings();

    /// <summary>
    /// 現在のパラメータを PrimitiveBuilder 呼び出しの C++ コード文字列に変換
    /// </summary>
    /// <returns>designated initializer 形式のコード（デフォルト値と同じフィールドは省略）</returns>
    std::string GeneratePrimitiveCppString() const;

    /// <summary>
    /// 現在のプリミティブ設定を JSON プリセットとして保存
    /// </summary>
    /// <param name="name">プリセット名（拡張子なし）</param>
    /// <returns>成功したら true</returns>
    bool SavePrimitivePreset(const std::string& name);

    /// <summary>
    /// JSON プリセットを読み込んで現在の設定へ反映
    /// </summary>
    /// <param name="name">プリセット名（拡張子なし）</param>
    /// <returns>成功したら true</returns>
    bool LoadPrimitivePreset(const std::string& name);

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
    class EmitterManager* emitterManager_               = nullptr;
    std::string           selectedEmitterName_;                     ///< 空 = 未選択
    char                  newEmitterNameBuffer_[128]    = "";
    char                  renameEmitterNameBuffer_[128] = "";
    char                  presetNameBuffer_[128]        = "";

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

    //パーティクルエディター 3ペイン/プレビュー
    ParticleInspectTarget            particleInspectTarget_       = ParticleInspectTarget::None;  ///< インスペクタ表示対象（最後にクリックしたリストで決まる）
    std::unique_ptr<PreviewViewport> particlePreviewViewport_;
    std::unique_ptr<Camera>          particlePreviewCamera_;
    OrbitCameraController            particleOrbitCamera_;
    bool                             particlePreviewSelectedOnly_ = false;                        ///< true = 選択エミッターのみ描画
    bool                             particlePreviewShowGrid_     = true;
    std::string                      particleSelectedPreset_;                                     ///< Load コンボの選択中プリセット名

    //パーティクルエディター 入力状態
    int     newEmitterTypeIndex_               = 0;
    Vector3 newEmitterPosition_                = { 0.0f, 0.0f, 0.0f };
    float   newEmitterSphereRadius_            = 1.0f;
    Vector3 newEmitterBoxSize_                 = { 1.0f, 1.0f, 1.0f };
    Vector3 newEmitterBoxRotation_             = { 0.0f, 0.0f, 0.0f };
    Vector3 newEmitterTriV1_                   = { -1.0f, 0.0f, 0.0f };
    Vector3 newEmitterTriV2_                   = { 1.0f, 0.0f, 0.0f };
    Vector3 newEmitterTriV3_                   = { 0.0f, 1.0f, 0.0f };
    int     newEmitterModelIndex_              = 0;
    char    newEmitterModelPathBuffer_[256]    = "";
    int     clipboardSlotIndex_                = 0;
    int     clipboardPasteModeIndex_           = 0;
    char    emitterTexturePathBuffer_[256]     = "";
    char    emitterRenderModelPathBuffer_[256] = "";
    char    scenePresetNameBuffer_[128]        = "scene_preset";
    Vector3 groupPositionEdit_                 = { 0.0f, 0.0f, 0.0f };   ///< 全グループ共有の編集値（実位置とは非同期の既存挙動を踏襲）
    int     groupAddEmitterIndex_              = 0;
    int     newForceFieldTypeIndex_            = 0;
    Vector3 newForceFieldPosition_             = { 0.0f, 0.0f, 0.0f };
    Vector3 newForceFieldDirection_            = { 0.0f, -1.0f, 0.0f };
    float   newForceFieldStrength_             = 1.0f;
    float   newForceFieldRadius_               = 0.0f;
    float   newForceFieldFalloff_              = 1.0f;

    //プリミティブエディター用
    PrimitiveType                    selectedPrimitiveType_     = PrimitiveType::Cylinder;
    PrimitiveBuilder::CubeParams     primCubeParams_{};
    PrimitiveBuilder::SphereParams   primSphereParams_{};
    PrimitiveBuilder::PlaneParams    primPlaneParams_{};
    PrimitiveBuilder::RingParams     primRingParams_{};
    PrimitiveBuilder::CylinderParams primCylinderParams_{};
    PrimitiveBuilder::TorusParams    primTorusParams_{};
    std::unique_ptr<Object3d>        primPreviewObject_;                 ///< プレビュー対象（エディタ表示中のみ生存）
    std::unique_ptr<Object3d>        primFloorObject_;                   ///< 床参照プレーン
    bool                             primParamsDirty_           = false; ///< 次の Update でモデル再生成（描画コマンド記録済みフレーム内での差し替えは危険）
    Vector3                          primPreviewRotate_         = {};
    Vector3                          primPreviewScale_          = { 1.0f, 1.0f, 1.0f };
    Vector4                          primPreviewColor_          = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool                             primPreviewLighting_       = true;
    bool                             primPreviewTransparent_    = false;
    bool                             primAutoRotate_            = false;
    float                            primAutoRotateSpeed_       = 1.0f;  ///< 自動回転速度（rad/s）
    bool                             primShowFloor_             = true;
    char                             primPresetNameBuffer_[128] = "";
    std::string                      primSelectedPreset_;                ///< Load コンボの選択中プリセット名
    float                            primMaterialShininess_     = 15.0f; ///< Mesh::CreateMaterialData の初期値と一致
    bool                             primMaterialHighlight_     = true;  ///< スペキュラ有効（Mesh 初期値と一致）
    std::string                      primMaterialTexture_;               ///< 空 = white.dds デフォルト
    Vector2                          primMaterialUvScale_       = { 1.0f, 1.0f };
    Vector2                          primMaterialUvOffset_      = { 0.0f, 0.0f };
    float                            primMaterialUvRotate_      = 0.0f;  ///< ラジアン
    char                             primExportNameBuffer_[128] = "";    ///< OBJ 出力名（拡張子なし）

    //プリミティブエディター専用ビューポート/カメラ
    std::unique_ptr<PreviewViewport> primPreviewViewport_;             ///< オフスクリーンRT一式（初回オープン時に生成）
    std::unique_ptr<Camera>          primPreviewCamera_;
    Camera*                          primPreviewCameraPtr_ = nullptr;  ///< Object3d::SetCamera(Camera**) に渡す安定アドレス
    OrbitCameraController            primOrbitCamera_;
  };

} // namespace Tako

#endif // _DEBUG