#pragma once

#ifdef _DEBUG

#include <memory>
#include <string>
#include "Vector3.h"
#include "Vector4.h"
#include "ViewportCameraController.h"

namespace Tako {

  class Camera;
  class PreviewViewport;
  class EmitterManager;
  class ForceFieldManager;
  class GPUParticleEmitter;
  struct ForceFieldData;

  /// <summary>
  /// GPU パーティクルのエミッター / フォースフィールド / グループを編集するエディタ（3ペイン: リスト / プレビュー / インスペクタ）
  /// </summary>
  class ParticleEditor {
  public: //構造体
    /// <summary>
    /// インスペクタ表示対象
    /// </summary>
    enum class InspectTarget : int {
      None = 0,
      Emitter,
      ForceField,
      Group
    };

  public: //メンバー関数
    ParticleEditor();
    ~ParticleEditor();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    /// <summary>
    /// GPU リソースを解放
    /// </summary>
    void Finalize();

    /// <summary>
    /// 更新（プレビュー RT/カメラの生成、オービットカメラ反映）
    /// GPU アイドル区間で呼ぶこと
    /// </summary>
    void Update();

    /// <summary>
    /// エディターウィンドウとメインシーンへの可視化線を描画
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool Draw();

    /// <summary>
    /// プレビューをオフスクリーン RT へ描画
    /// TakoFramework::Draw() の GPUParticle::Draw() 後・ImGui 描画前に呼ぶ
    /// </summary>
    void DrawPreviewPass();

    //============================================================
    //Setter
    //============================================================
    void SetEmitterManager(EmitterManager* emitterManager) { emitterManager_ = emitterManager; }
    void SetForceFieldManager(ForceFieldManager* forceFieldManager) { forceFieldManager_ = forceFieldManager; }

  private: //非公開関数
    /// <summary>
    /// 左ペイン: エミッター / フォースフィールド / グループの各セクションを描画
    /// </summary>
    void DrawListPane();

    /// <summary>
    /// 左ペイン: エミッター作成フォーム / 一覧 / Delete・Duplicate / クリップボード
    /// </summary>
    void DrawEmitterSection();

    /// <summary>
    /// 左ペイン: フォースフィールド追加フォーム / 一覧
    /// </summary>
    void DrawForceFieldSection();

    /// <summary>
    /// 左ペイン: グループ作成 / 一覧
    /// </summary>
    void DrawGroupSection();

    /// <summary>
    /// 中央ペイン: プレビューツールバー + プレビュー画像 + オービットカメラ入力
    /// </summary>
    /// <returns>プレビューがホバー中または操作中なら true</returns>
    bool DrawPreviewPane();

    /// <summary>
    /// 右ペイン: 選択対象別インスペクタ + 共通セクション（Scene Presets / Visualization）
    /// </summary>
    void DrawInspectorPane();

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
    void DrawVisualization();

    /// <summary>
    /// パーティクル可視化設定UIの描画（メインシーンへの線描画の ON/OFF と色）
    /// </summary>
    void DrawVisualizationSettings();

    /// <summary>
    /// 個別エミッターの形状を描画
    /// </summary>
    /// <param name="emitter">描画対象のエミッター</param>
    void DrawEmitterShape(const std::shared_ptr<GPUParticleEmitter>& emitter);

    /// <summary>
    /// 個別フォースフィールドの可視化を描画
    /// </summary>
    /// <param name="field">描画対象のフォースフィールド</param>
    /// <param name="index">フィールドのインデックス（ハイライト判定用）</param>
    void DrawForceFieldVisualization(const ForceFieldData& field, int index);

  private: //メンバー変数
    bool* isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）

    //エミッター管理用
    EmitterManager* emitterManager_               = nullptr;
    std::string     selectedEmitterName_;                     ///< 空 = 未選択
    char            newEmitterNameBuffer_[128]    = "";
    char            renameEmitterNameBuffer_[128] = "";
    char            presetNameBuffer_[128]        = "";

    //グループ管理用
    int  selectedGroupIndex_      = -1;
    char newGroupNameBuffer_[128] = "";

    //フォースフィールド管理用
    ForceFieldManager* forceFieldManager_       = nullptr;
    int                selectedForceFieldIndex_ = -1;
    char               ffPresetSaveBuffer_[128] = "";
    char               ffPresetLoadBuffer_[128] = "";
    std::string        currentFFPresetName_;  ///< 最後にロード/保存したFFプリセット名（上書き保存用）

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

    //3ペイン/プレビュー
    InspectTarget                    inspectTarget_       = InspectTarget::None;  ///< インスペクタ表示対象（最後にクリックしたリストで決まる）
    std::unique_ptr<PreviewViewport> previewViewport_;
    std::unique_ptr<Camera>          previewCamera_;
    ViewportCameraController         cameraController_;
    bool                             previewSelectedOnly_ = false;                ///< true = 選択エミッターのみ描画
    bool                             previewShowGrid_     = true;
    std::string                      selectedPreset_;                             ///< Load コンボの選択中プリセット名

    //入力状態
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
  };

} // namespace Tako

#endif // _DEBUG
