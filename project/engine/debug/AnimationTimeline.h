#pragma once

#ifdef _DEBUG

#include "AnimationClip.h"
#include "imgui.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Tako {

  /// <summary>
  /// AnimationClip のドープシートと、選択トラックの値グラフ。キーとイベントの追加・移動は clip を直接書き換える
  /// </summary>
  class AnimationTimeline {
  private: //定数
    static constexpr float kRulerHeight        = 22.0f;
    static constexpr float kRowHeight          = 22.0f;
    static constexpr float kLabelWidth         = 200.0f;
    static constexpr float kKeySize            = 6.0f;     ///< キーの菱形の半径（px）
    static constexpr float kMinPixelsPerSecond = 40.0f;
    static constexpr float kMaxPixelsPerSecond = 4000.0f;

  private: //構造体
    /// <summary>
    /// 左ボタン・中ボタンで進行中の操作
    /// </summary>
    enum class Operation {
      None,
      Scrub,
      Move,
      CurveMove,    ///< 値グラフでキーの値と時刻を動かす
      CurveHandle,  ///< 値グラフで CubicBezier の制御点を動かす
      RectSelect,
      Pan
    };

    /// <summary>
    /// 移動を始めたときの時刻と値（並び替えで位置が変わるので id で引き直す）
    /// </summary>
    struct DragItem {
      uint32_t id;
      float    startTime;
      float    startValue = 0.0f;  ///< 値グラフで動かすときだけ使う
    };

    /// <summary>
    /// 値グラフの縦の範囲と、値と画面の y の変換
    /// </summary>
    struct CurveView {
      float originX  = 0.0f;  ///< 見出し列の左端（スクリーン座標）
      float top      = 0.0f;
      float bottom   = 0.0f;
      float minValue = 0.0f;
      float maxValue = 1.0f;

      [[nodiscard]] float ValueToY(float value) const { return bottom - (value - minValue) / (maxValue - minValue) * (bottom - top); }
      [[nodiscard]] float YToValue(float y) const { return minValue + (bottom - y) / (bottom - top) * (maxValue - minValue); }
    };

    /// <summary>
    /// 値グラフに出す CubicBezier の制御点 1 つ。時刻と値はグラフ上の位置
    /// </summary>
    struct BezierHandle {
      uint32_t keyId;        ///< 区間の始めのキー（ease を持つ方）
      int      index;        ///< 1 なら bezierP1、2 なら bezierP2
      float    anchorTime;   ///< 線の根元のキー（P1 は区間の始め、P2 は終わり）
      float    anchorValue;
      float    time;
      float    value;
    };

  public: //メンバー関数
    /// <summary>
    /// ドープシートを高さ height で描き、マウス操作を受ける
    /// </summary>
    void Draw(AnimationClip& clip, float& playhead, float height);

    /// <summary>
    /// 選択中のトラックの値の変化を、ドープシートと同じ時間軸で描く。キーのドラッグで値と時刻を、ダブルクリックでキーを足す
    /// </summary>
    void DrawCurve(AnimationClip& clip, float& playhead);

    /// <summary>
    /// 削除・Undo・読み直しで無くなったキーとイベントを選択から外す
    /// </summary>
    void PruneSelection(AnimationClip& clip);

    void ClearSelection() { selection_.clear(); }

    /// <summary>
    /// 全てのキーとイベントを選ぶ
    /// </summary>
    void SelectAll(const AnimationClip& clip);

    /// <summary>
    /// 次の Draw で clip 全体が収まる表示範囲にする（開き直したとき用）
    /// </summary>
    void RequestFit() { isFitRequested_ = true; }

    /// <summary>
    /// 時刻をスナップ間隔へ丸める（間隔が 0 なら丸めない）
    /// </summary>
    [[nodiscard]] float Snap(float time) const;

    /// <summary>
    /// トラックの見出し（"対象  プロパティ"。対象が空なら "(Bound)"）
    /// </summary>
    [[nodiscard]] static std::string TrackLabel(const AnimTrack& track);

    //============================================================
    //Setter
    //============================================================
    void SetSnapInterval(float interval) { snapInterval_ = interval; }
    void SetSelectedTrack(int index) { selectedTrack_ = index; }
    void SetSelection(std::vector<uint32_t> ids) { selection_ = std::move(ids); }

    //============================================================
    //Getter
    //============================================================
    [[nodiscard]] float GetSnapInterval() const { return snapInterval_; }

    /// <summary>
    /// 選択中のトラックの添字。無ければ -1
    /// </summary>
    [[nodiscard]] int GetSelectedTrack() const { return selectedTrack_; }

    /// <summary>
    /// 選択中のキーとイベントの id（キーとイベントは同じ番号を使わない）
    /// </summary>
    [[nodiscard]] const std::vector<uint32_t>& GetSelection() const { return selection_; }

    [[nodiscard]] bool IsSelected(uint32_t id) const;

  private: //非公開関数
    void DrawRows(const AnimationClip& clip, ImDrawList* drawList);
    void DrawRuler(const AnimationClip& clip, ImDrawList* drawList, float playhead);
    void HandleInput(AnimationClip& clip, float& playhead);
    void HandleCurveInput(AnimationClip& clip, AnimTrack& track, const std::vector<BezierHandle>& handles, float& playhead, bool isHovered);

    /// <summary>
    /// Ctrl+ホイールの拡大縮小、Shift+ホイールの横スクロール、中ボタンのパン。originX はキーを並べる列の左にある見出し列の左端
    /// </summary>
    /// <returns>パン中なら true（ほかの操作を受けない）</returns>
    bool HandleViewInput(float originX, float duration, bool isHovered);

    /// <summary>
    /// キーかイベントを押したときの選択。Ctrl なら足し引きし、選択中のものを押したら離すまで複数選択を保つ
    /// </summary>
    void SelectOnPress(uint32_t id, bool isToggle);

    /// <summary>
    /// 左ボタンを離したときに、進行中の操作を終える（何度呼んでも同じ）
    /// </summary>
    void FinishDrag(const AnimationClip& clip);

    /// <summary>
    /// 行 row の時刻 x 付近にあるキーかイベントの id
    /// </summary>
    /// <returns>無ければ 0</returns>
    uint32_t HitTest(const AnimationClip& clip, int row, float x) const;

    /// <summary>
    /// 値グラフで position の付近にある track のキーの id
    /// </summary>
    /// <returns>無ければ 0</returns>
    uint32_t HitTestCurve(const AnimTrack& track, ImVec2 position) const;

    /// <summary>
    /// 選択中のキーにつながる CubicBezier の区間の制御点
    /// </summary>
    std::vector<BezierHandle> BezierHandles(const AnimTrack& track) const;

    /// <summary>
    /// 値グラフで position の付近にある制御点
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    const BezierHandle* HitTestHandle(const std::vector<BezierHandle>& handles, ImVec2 position) const;

    /// <summary>
    /// ダブルクリックした行の time にキー（イベント行ならイベント）を足して選ぶ
    /// </summary>
    void AddAt(AnimationClip& clip, int row, float time);

    void BeginMove(AnimationClip& clip, uint32_t anchorId);
    void UpdateMove(AnimationClip& clip, float& playhead);

    /// <summary>
    /// 値グラフで、track の選択中のキーを動かし始める
    /// </summary>
    void BeginCurveMove(const AnimTrack& track, uint32_t anchorId);

    /// <summary>
    /// 縦の移動で値を、横の移動で時刻を動かす。Shift なら多く動いた方の軸だけを使う
    /// </summary>
    void UpdateCurveMove(AnimationClip& clip, AnimProperty property, float& playhead);

    /// <summary>
    /// 掴んだ制御点を、区間の中の割合（x は時間で 0～1、y は進み具合）で動かす
    /// </summary>
    void UpdateHandleMove(AnimTrack& track);

    [[nodiscard]] bool IsCurveDragging() const { return operation_ == Operation::CurveMove || operation_ == Operation::CurveHandle; }
    void FinishRectSelect(const AnimationClip& clip);

    /// <summary>
    /// 画面上の y にある行。-1 は目盛り、0 はイベント、1 以降はトラック（添字 + 1）。範囲外は -2
    /// </summary>
    [[nodiscard]] int RowAt(float screenY) const;

    [[nodiscard]] float RowTop(int row) const { return origin_.y + kRulerHeight + kRowHeight * static_cast<float>(row); }
    [[nodiscard]] float TimeToX(float time) const { return origin_.x + kLabelWidth + (time - offset_) * pixelsPerSecond_; }
    [[nodiscard]] float XToTime(float screenX) const { return (screenX - origin_.x - kLabelWidth) / pixelsPerSecond_ + offset_; }
    [[nodiscard]] float CurveTimeToX(float time) const { return curveView_.originX + kLabelWidth + (time - offset_) * pixelsPerSecond_; }
    [[nodiscard]] float CurveXToTime(float screenX) const { return (screenX - curveView_.originX - kLabelWidth) / pixelsPerSecond_ + offset_; }

    /// <summary>
    /// 目盛りの間隔（ラベルが重ならない最小の切りのよい値）
    /// </summary>
    [[nodiscard]] float RulerStep() const;

    void ClampOffset(float duration);

    /// <summary>
    /// id のキーかイベントの時刻
    /// </summary>
    /// <returns>無ければ nullptr</returns>
    static float* FindTime(AnimationClip& clip, uint32_t id);

    static ImU32 TrackColor(AnimProperty property);

  private: //メンバー変数
    //表示範囲
    float  pixelsPerSecond_ = 400.0f;
    float  offset_          = 0.0f;    ///< 左端の時刻（秒）
    float  snapInterval_    = 0.05f;   ///< 秒。0 ならスナップしない
    bool   isFitRequested_  = true;
    ImVec2 origin_          = {};      ///< 直近の Draw の目盛り左上（スクリーン座標）
    float  width_           = 0.0f;

    //選択
    std::vector<uint32_t> selection_;
    int                   selectedTrack_ = -1;
    int                   contextTrack_  = -1;  ///< 右クリックメニューを開いたトラック

    //操作
    Operation             operation_      = Operation::None;
    ImVec2                dragStart_      = {};
    ImVec2                dragCurrent_    = {};
    float                 panStartOffset_ = 0.0f;
    std::vector<DragItem> dragItems_;
    float                 dragAnchorTime_ = 0.0f;
    uint32_t              pendingSingle_  = 0;                ///< 選択中のものを押して動かさずに離したら、これだけを選ぶ

    //値グラフ
    CurveView curveView_;           ///< 直近の DrawCurve の範囲。値グラフでドラッグしている間は縦の範囲を変えない
    uint32_t  curveAnchorId_ = 0;   ///< 値グラフで掴んだキー
    uint32_t  handleKeyId_   = 0;   ///< 掴んだ制御点を持つキー
    int       handleIndex_   = 0;   ///< 掴んだ制御点（1 なら bezierP1、2 なら bezierP2）
    Vector2   handleStart_   = {};  ///< 掴んだときの制御点
  };

} // namespace Tako

#endif // _DEBUG
