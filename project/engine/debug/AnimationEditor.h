#pragma once

#ifdef _DEBUG

#include "AnimationClip.h"
#include "AnimationTimeline.h"
#include "EditTracker.h"
#include "PreviewPanel.h"
#include "ShortcutTable.h"
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Tako {

  class Object3d;
  class UILayout;

  /// <summary>
  /// AnimationClip をドープシートとインスペクターで編集するエディタ。
  /// クリップは名前ごとに共有のインスタンスなので、編集はゲーム側の再生にもすぐ反映される
  /// </summary>
  class AnimationEditor {
  private: //定数
    static constexpr float kSpeedOptions[] = { 0.25f, 0.5f, 1.0f, 2.0f };
    static constexpr float kSnapOptions[]  = { 0.0f, 0.01f, 0.02f, 0.05f, 0.1f, 0.25f };  ///< 秒。0 はスナップなし
    static constexpr float kPreviewHeight  = 260.0f;                                      ///< 3D プレビュー画像の高さ（px）

  private: //構造体
    /// <summary>
    /// 名前を入力するポップアップの用途
    /// </summary>
    enum class NamePopup {
      None,
      New,      ///< 空のクリップを作る
      SaveCopy  ///< 今の内容を別名で保存する
    };

    /// <summary>
    /// プレビューで動かす対象
    /// </summary>
    enum class PreviewMode {
      UI,       ///< UI レイアウトの要素（ゲーム画面に表示）
      Object3D  ///< エディタ内の 3D プレビューのモデル
    };

    /// <summary>
    /// コピーしたキーと、元のトラックの添字
    /// </summary>
    struct CopiedKey {
      size_t  track;
      AnimKey key;
    };

  public: //メンバー関数
    AnimationEditor();
    ~AnimationEditor();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    /// <param name="cameraSettings">3D プレビューのカメラ操作の設定（EngineSettingsWindow 所有）</param>
    void Initialize(bool* isOpen, const ViewportCameraSettings* cameraSettings);

    /// <summary>
    /// プレビューの再生と対象への適用。シーン更新の後に毎フレーム呼ぶ
    /// </summary>
    void Update();

    /// <summary>
    /// ウィンドウを描く
    /// </summary>
    /// <returns>3D プレビューがホバー中か操作中なら true（ゲーム入力の遮断に使う）</returns>
    bool Draw();

    /// <summary>
    /// シーンに無いレイアウトでプレビュー中なら、エディタが読み込んだレイアウトをゲーム画面の上に描く
    /// </summary>
    void DrawLayoutPreview();

    /// <summary>
    /// 3D プレビューをオフスクリーン RT へ描く
    /// </summary>
    void DrawPreviewPass();

    void Finalize();

    /// <summary>
    /// 編集中のクリップに未保存の変更があれば out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<AnimationEditor>> Shortcuts();

  private: //非公開関数
    void DrawMenuBar();
    void DrawToolbar();
    void DrawNamePopup();
    void DrawPreviewTarget();
    void DrawPlaybackControls();
    void DrawAddTrackPopup();
    void DrawInspector();
    void DrawKeyInspector();
    void DrawStatusBar();

    /// <summary>
    /// 名前のクリップを開く
    /// </summary>
    /// <returns>ファイルが無く開けなければ false</returns>
    bool Open(const std::string& name);

    /// <summary>
    /// 未保存なら確認してから name を開く
    /// </summary>
    void RequestOpen(const std::string& name);

    /// <summary>
    /// 未保存なら確認してからファイルの内容に戻す
    /// </summary>
    void RequestReload();

    /// <summary>
    /// 未保存の変更をファイルの内容に戻す（共有のクリップなので、捨てた内容をゲームが再生し続けないように）
    /// </summary>
    void RevertUnsaved();

    /// <summary>
    /// 名前入力ポップアップを次のフレームで開く
    /// </summary>
    void OpenNamePopup(NamePopup purpose);

    bool Save();

    /// <summary>
    /// 今の内容を別名のファイルへ書く。編集中のクリップはそのまま
    /// </summary>
    bool SaveCopy(const std::string& name);

    /// <summary>
    /// 空のクリップをファイルに作って開く
    /// </summary>
    bool CreateNew(const std::string& name);

    /// <summary>
    /// name のファイルがあるか（New / Save Copy As で上書きしないため）
    /// </summary>
    bool IsNameTaken(const std::string& name) const;

    /// <summary>
    /// directory の JSON のファイル名（拡張子なし、名前順）
    /// </summary>
    static std::vector<std::string> ListJsonFiles(const char* directory);

    /// <summary>
    /// シーン（エディタが読み込んだもの以外）の name のレイアウト
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    UILayout* FindSceneLayout(const std::string& name) const;

    /// <summary>
    /// プレビューするレイアウト。シーンが読み込んでいればそのインスタンス、無ければエディタが読み込んだもの
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    UILayout* FindTargetLayout() const;

    /// <summary>
    /// プレビューするレイアウトの要素名（ツリー順）
    /// </summary>
    std::vector<std::string> ElementNames() const;

    /// <summary>
    /// 再生ヘッドの姿勢をレイアウトに渡す。F7 で止めていても見えるよう Update もここで呼ぶ
    /// </summary>
    void UpdateLayoutPreview();

    /// <summary>
    /// 直前までプレビューしていたレイアウトをゲーム側の再生に戻す
    /// </summary>
    void ReleaseLayoutPreview();

    void UpdateObjectPreview();

    /// <summary>
    /// target の properties のトラックを足す（既にあるものは足さない）。再生ヘッドに静止時の値のキーを 1 つ置く
    /// </summary>
    void AddTracks(const std::string& target, std::initializer_list<AnimProperty> properties);

    /// <summary>
    /// インスペクターに出す値の単位
    /// </summary>
    const char* UnitOf(AnimProperty property) const;

    void TogglePlay();

    /// <summary>
    /// 選択中のトラックの再生ヘッドの位置に、今の値のキーを打つ（既にあればそれを選ぶ）
    /// </summary>
    void AddKeyAtPlayhead();

    void CopySelection();

    /// <summary>
    /// コピーしたキーを、最初のキーが再生ヘッドに来るようずらして元のトラックへ貼る
    /// </summary>
    void Paste();

    void DeleteSelection();

    /// <summary>
    /// プレビューを止め、対象の AnimationPlayer で実際に再生する（遅延とイベントの確認用）
    /// </summary>
    void PlayOnTarget();

    void StopOnTarget();

  private: //メンバー変数
    bool* isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）

    //編集対象
    std::string            name_;                 ///< 開いているクリップ名
    AnimationClip*         clip_      = nullptr;  ///< AnimationClip::Find の共有インスタンス
    AnimationTimeline      timeline_;
    std::vector<CopiedKey> clipboard_;

    //再生（ゲームの再生とは別に、エディタだけの時刻で動かす）
    float playhead_         = 0.0f;   ///< 秒
    float playElapsed_      = 0.0f;   ///< 再生開始からの秒。ループの種類に従って playhead_ にする
    bool  isPlaying_        = false;
    bool  isPreviewEnabled_ = true;   ///< 再生ヘッドの姿勢を対象に上乗せするか
    float previewSpeed_     = 1.0f;

    //プレビューの対象
    PreviewMode               previewMode_       = PreviewMode::UI;
    std::string               layoutName_;
    std::string               pendingLayoutName_;                    ///< 描画中に読み込むとスプライトを作り直すので、次の Update で切り替える
    std::string               boundTarget_;                          ///< target が空のトラックを当てる要素名。空はルート
    std::unique_ptr<UILayout> ownedLayout_;                          ///< シーンが読み込んでいないレイアウトを開いたときのインスタンス
    UILayout*                 previewedLayout_   = nullptr;          ///< SetPreview を渡したレイアウト（戻すため）

    //3D プレビュー
    PreviewPanel              preview_;
    std::unique_ptr<Object3d> previewObject_;
    std::unique_ptr<Object3d> floorObject_;
    std::string               previewModel_;          ///< ModelManager のモデル名。空なら立方体
    bool                      isModelDirty_  = true;  ///< 描画中にモデルを差し替えると記録済みのコマンドが参照先を失うので、Update で差し替える

    //Play on Target
    float playDelay_ = 0.0f;  ///< 秒

    //未保存と Undo
    EditTracker<>        tracker_{ { [this] { return clip_ ? clip_->ToJson().dump() : std::string(); },
                                     [this](const std::string& state) { if (clip_) clip_->FromJson(nlohmann::json::parse(state)); } } };
                                     UnsavedChangesPrompt                                                                             unsavedPrompt_;  ///< 未保存のまま別のクリップを開く・読み直すときの確認

    //名前入力ポップアップ
    NamePopup namePopup_            = NamePopup::None;
    bool      isNamePopupRequested_ = false;            ///< メニューの中で OpenPopup するとメニュー側の ID になるので、メニューを閉じてから開く
    char      nameBuffer_[128]      = "";

    //トラック追加ポップアップ
    std::string addTrackTarget_;  ///< 選んだ対象。空は Bound
  };

} // namespace Tako

#endif // _DEBUG
