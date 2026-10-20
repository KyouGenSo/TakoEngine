#pragma once

#ifdef _DEBUG

#include "CameraAnimation.h"
#include "CameraKeyframe.h"
#include "CameraAnimationTimeline.h"
#include "CameraAnimationCurveEditor.h"
#include "EditTracker.h"
#include "ShortcutTable.h"
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Tako {

  class DebugViewport;

  /// <summary>
  /// CameraAnimation の JSON をタイムライン・カーブ・インスペクターで編集するエディタ。
  /// シーンが読み込んだアニメーションはそのインスタンスを直接編集し、読み込んでいないファイルはエディタ自前のインスタンスで開く
  /// </summary>
  class CameraAnimationEditor {
  private: //定数
    static constexpr int kPathDivision = 24;    ///< 経路線の描画で1区間を何本の線分に分けるか

  private: //構造体
    /// <summary>
    /// 名前を入力するポップアップの用途
    /// </summary>
    enum class NamePopup {
      None,
      New,      ///< 空のアニメーションを作る
      SaveCopy  ///< 今の内容を別名で保存する
    };

    /// <summary>
    /// キーフレームに取り込むカメラ
    /// </summary>
    enum class CaptureSource {
      MainCamera,
      DebugViewport
    };

  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    /// <param name="debugViewport">カメラの取り込み元にできる Debug Viewport（DebugUIManager 所有）</param>
    void Initialize(bool* isOpen, const DebugViewport* debugViewport) {
      isOpen_        = isOpen;
      debugViewport_ = debugViewport;
    }

    /// <summary>
    /// 編集中のインスタンスの生存確認と、プレビューの再生・カメラへの適用。シーン更新の後に毎フレーム呼ぶ
    /// </summary>
    void Update();

    void Draw();

    /// <summary>
    /// 編集中のアニメーションに未保存の変更があれば out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<CameraAnimationEditor>> Shortcuts();

  private: //非公開関数
    void DrawMenuBar();
    void DrawToolbar();
    void DrawNamePopup();
    void DrawPlaybackControls();
    void DrawTimelinePanel();
    void DrawInspectorPanel();
    void DrawStatusBar();

    /// <summary>
    /// 編集中アニメーションの位置経路を LineRenderer で3D空間へ描画
    /// </summary>
    void DrawPathVisualization();

    /// <summary>
    /// 名前のアニメーションを開く。シーンが読み込んでいればそのインスタンス、無ければファイルから自前で読み込む
    /// </summary>
    /// <returns>開けたら true</returns>
    bool Open(const std::string& name);

    /// <summary>
    /// 未保存なら確認してから name を開く
    /// </summary>
    void RequestOpen(const std::string& name);

    /// <summary>
    /// 未保存の変更を破棄したシーンのインスタンスをファイルの内容に戻す（ゲームが未保存の内容を再生し続けないように）
    /// </summary>
    void RevertSceneAnimation();

    /// <summary>
    /// 名前入力ポップアップを次のフレームで開く
    /// </summary>
    void OpenNamePopup(NamePopup purpose);

    /// <summary>
    /// 未保存なら確認してからファイルの内容に戻す
    /// </summary>
    void RequestReload();

    bool Save();

    /// <summary>
    /// 今の内容を別名のファイルへ書く。編集中のアニメーションはそのまま
    /// </summary>
    bool SaveCopy(const std::string& name);

    /// <summary>
    /// 空のアニメーションをファイルに作って開く
    /// </summary>
    bool CreateNew(const std::string& name);

    /// <summary>
    /// name のファイルかシーンのインスタンスがあるか（New / Save Copy As で上書きしないため）
    /// </summary>
    bool IsNameTaken(const std::string& name) const;

    /// <summary>
    /// シーン（エディタ自前以外）が読み込んだ name のアニメーション
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    CameraAnimation* FindSceneAnimation(const std::string& name) const;

    /// <summary>
    /// kDirectory の JSON のファイル名（拡張子なし、名前順）
    /// </summary>
    static std::vector<std::string> ListAnimationFiles();

    /// <summary>
    /// メインカメラ（Renderer のカメラ）
    /// </summary>
    static Camera* MainCamera();

    /// <summary>
    /// captureSource_ で選んだカメラ
    /// </summary>
    /// <returns>無ければ nullptr（Debug Viewport は閉じている間カメラを持たない）</returns>
    const Camera* CaptureSourceCamera() const;

    /// <summary>
    /// 時刻をグリッド間隔へ丸める
    /// </summary>
    /// <param name="time">入力時刻（秒）</param>
    /// <returns>スナップ有効時は丸めた時刻、無効時は time をそのまま返す</returns>
    float SnapToGrid(float time) const;

    /// <summary>
    /// CaptureSourceCamera の今の状態のキーフレーム。TARGET_RELATIVE ならターゲットからのオフセットにする
    /// </summary>
    CameraKeyframe CaptureCamera(float time, CameraKeyframe::CoordinateType coordinateType) const;

    void TogglePlay();
    void CopySelectedKeyframes();
    void PasteKeyframes();
    void DeleteSelectedKeyframes();

  private: //メンバー変数
    bool*                isOpen_        = nullptr;                    ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    const DebugViewport* debugViewport_ = nullptr;
    bool                 showPath_      = true;                       ///< カメラ経路を3D空間に線で表示するか
    CaptureSource        captureSource_ = CaptureSource::MainCamera;

    //編集対象
    std::string                      name_;                      ///< 開いているファイル名（拡張子なし）
    CameraAnimation*                 animation_      = nullptr;  ///< シーンのインスタンスか ownedAnimation_
    std::unique_ptr<CameraAnimation> ownedAnimation_;            ///< シーンが読み込んでいないファイルを開いたときのインスタンス

    //UI コンポーネント
    CameraAnimationTimeline    timeline_;
    CameraAnimationCurveEditor curveEditor_;

    //選択状態
    std::vector<uint32_t> selectedKeyframes_;  ///< タイムラインの選択（キーフレームの id）の写し（描画ごとに更新）。変えるときは timeline_ 側を変える

    //タイムライン設定
    float gridSnapInterval_ = 0.1f;  ///< 秒
    bool  enableGridSnap_   = true;

    std::vector<CameraKeyframe> clipboard_;

    //プレビュー（ゲームの再生状態とは別に、エディタだけの時刻で動かす）
    float                                playhead_            = 0.0f;   ///< 秒
    bool                                 isPlaying_           = false;
    bool                                 isPreviewEnabled_    = false;  ///< 再生ヘッドの姿勢をメインカメラへ書くか
    std::optional<CameraAnimation::Pose> cameraBeforePreview_;          ///< プレビュー開始時のメインカメラ。終えたら戻す
    CameraAnimation::Pose                lastPreviewPose_;              ///< 直近にメインカメラへ書いた姿勢。終えたとき、ゲームが書き直した値は戻さない

    //未保存と Undo
    EditTracker<>        tracker_{ { [this] { return animation_ ? animation_->ToJson().dump() : std::string(); },
                                     [this](const std::string& state) { if (animation_) animation_->FromJson(nlohmann::json::parse(state)); } } };
    UnsavedChangesPrompt unsavedPrompt_;  ///< 未保存のまま別のアニメーションを開く・読み直すときの確認

    //名前入力ポップアップ
    NamePopup namePopup_            = NamePopup::None;
    bool      isNamePopupRequested_ = false;            ///< メニューの中で OpenPopup するとメニュー側の ID になるので、メニューを閉じてから開く
    char      nameBuffer_[128]      = "";
  };

} // namespace Tako

#endif // _DEBUG
