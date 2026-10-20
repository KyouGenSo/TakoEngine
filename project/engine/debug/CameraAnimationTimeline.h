#pragma once

#ifdef _DEBUG

#include "CameraAnimation.h"
#include "CameraKeyframe.h"
#include <utility>
#include <vector>
#include "ImGuiManager.h"

namespace Tako {

  /// <summary>
  /// キーフレームを視覚表示・操作するタイムライン UI
  /// </summary>
  class CameraAnimationTimeline {
  public: //構造体
    enum class TrackType {
      SUMMARY,        ///< 全キーフレームをまとめて表示
      POSITION_X,
      POSITION_Y,
      POSITION_Z,
      ROTATION_X,
      ROTATION_Y,
      ROTATION_Z,
      FOV,
      COUNT
    };

    enum class KeyframeStyle {
      DIAMOND,
      CIRCLE,
      SQUARE,
      TRIANGLE
    };

  public: //メンバー関数
    CameraAnimationTimeline();
    ~CameraAnimationTimeline();

    void Initialize(CameraAnimation* animation);

    /// <summary>
    /// タイムラインを描画。ルーラーのスクラブとキーフレームのクリック・ドラッグで playhead を動かす
    /// </summary>
    /// <param name="playhead">再生ヘッドの時刻（秒）。エディタが持つ</param>
    void Draw(float& playhead);

    void Update(float deltaTime);
    void ClearSelection();
    void SelectAll();
    bool IsSelected(uint32_t id) const;

    //==========================================================================
    //Setter
    //==========================================================================
    void SetKeyframeStyle(KeyframeStyle style) { keyframeStyle_ = style; }
    void SetTrackVisible(TrackType track, bool visible);
    void SetGridSnapEnabled(bool enable) { enableGridSnap_ = enable; }
    void SetGridSnapInterval(float interval) { gridSnapInterval_ = interval; }
    void SetZoom(float zoom);
    void SetOffset(float offset);

    //==========================================================================
    //Getter
    //==========================================================================
    /// <summary>
    /// 選択中のキーフレームの id（CameraKeyframe::id）
    /// </summary>
    const std::vector<uint32_t>& GetSelectedKeyframes() const { return selectedKeyframes_; }
    int GetHoveredKeyframe() const { return hoveredKeyframe_; }
    bool IsDragging() const { return isDragging_; }
    bool IsScrubbing() const { return isScrubbing_; }
    float GetZoom() const { return zoom_; }
    float GetOffset() const { return offset_; }

  private: //非公開関数
    void DrawTimeRuler();
    void DrawGrid();
    void DrawPlayhead(float playhead);
    void DrawTrack(TrackType trackType, float yPos);
    void DrawKeyframe(float xPos, float yPos, bool isSelected, bool isHovered);
    void DrawSelectionRect();
    void HandleMouseInput(float& playhead);

    /// <summary>
    /// 時間→スクリーン X 座標
    /// </summary>
    /// <param name="time">時刻（秒）</param>
    /// <returns>キャンバス基準のローカル X 座標（ピクセル）。zoom_/offset_ を反映</returns>
    float TimeToScreenX(float time) const;

    /// <summary>
    /// スクリーン X 座標→時間
    /// </summary>
    /// <param name="x">キャンバス基準のローカル X 座標（ピクセル）</param>
    /// <returns>時刻（秒）。zoom_/offset_ を反映</returns>
    float ScreenXToTime(float x) const;

    /// <summary>
    /// (x, y) にあるキーフレーム index を返す
    /// </summary>
    /// <param name="x">キャンバス基準のローカル X 座標（ピクセル）</param>
    /// <param name="y">キャンバス基準のローカル Y 座標（ピクセル）</param>
    /// <param name="trackType">判定対象トラック</param>
    /// <returns>当たったキーフレーム index。なければ -1</returns>
    int HitTestKeyframe(float x, float y, TrackType trackType) const;

    void ProcessRectSelection();

    /// <summary>
    /// 選択中のキーフレームをマウスの移動量だけずらす。クリックしたキーフレームには playhead も追従させる
    /// </summary>
    void ProcessKeyframeDrag(float& playhead);

    float SnapToGrid(float time) const;
    const char* GetTrackName(TrackType track) const;
    ImU32 GetTrackColor(TrackType track) const;
    void ClampOffset();

  private: //メンバー変数
    CameraAnimation* animation_ = nullptr;

    //UI 設定
    float timelineHeight_  = 300.0f;
    float trackHeight_     = 30.0f;
    float rulerHeight_     = 25.0f;
    float trackLabelWidth_ = 100.0f;
    float keyframeSize_    = 10.0f;

    //スタイル設定
    KeyframeStyle keyframeStyle_ = KeyframeStyle::DIAMOND;
    ImU32         gridColor_     = IM_COL32(60, 60, 60, 255);
    ImU32         playheadColor_ = IM_COL32(255, 100, 100, 255);
    ImU32         selectedColor_ = IM_COL32(255, 200, 100, 255);
    ImU32         hoveredColor_  = IM_COL32(200, 200, 255, 255);

    //選択状態
    std::vector<uint32_t> selectedKeyframes_;                       ///< キーフレームの id
    int                   hoveredKeyframe_   = -1;                  ///< 添字（毎フレーム求め直す）
    TrackType             hoveredTrack_      = TrackType::SUMMARY;

    //ドラッグ状態
    bool                                    isDragging_            = false;
    bool                                    isRectSelecting_       = false;
    ImVec2                                  dragStartPos_;
    ImVec2                                  dragCurrentPos_;
    std::vector<std::pair<uint32_t, float>> dragStartTimes_;                 ///< ドラッグ開始時の各キーフレームの id と時刻
    uint32_t                                dragAnchorId_          = 0;      ///< クリックしたキーフレームの id（playhead が追従する）
    uint32_t                                pendingSingleSelectId_ = 0;      ///< 選択中のものを押した。動かさずに離したらこれだけを選ぶ

    bool isScrubbing_ = false;

    //ズーム・スクロール状態
    float zoom_            = 1.0f;
    float offset_          = 0.0f;   ///< スクロール位置（秒）
    float dragStartOffset_ = 0.0f;
    bool  isPanning_       = false;  ///< 中ボタンドラッグ中

    //グリッドスナップ
    bool  enableGridSnap_   = true;
    float gridSnapInterval_ = 0.1f;  ///< 秒

    bool trackVisible_[static_cast<int>(TrackType::COUNT)];

    float minZoom_ = 0.1f;
    float maxZoom_ = 10.0f;

    float hoverAnimTime_ = 0.0f;  ///< ホバー拍動アニメの位相
  };

} // namespace Tako

#endif // _DEBUG
