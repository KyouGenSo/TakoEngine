#ifdef _DEBUG

#include "AnimationTimeline.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace Tako {

  namespace {

    constexpr ImU32 kBackgroundColor   = IM_COL32(36, 36, 36, 255);
    constexpr ImU32 kRowColor          = IM_COL32(44, 44, 44, 255);
    constexpr ImU32 kRowAltColor       = IM_COL32(40, 40, 40, 255);
    constexpr ImU32 kSelectedRowColor  = IM_COL32(52, 60, 78, 255);
    constexpr ImU32 kLabelColor        = IM_COL32(56, 56, 56, 255);
    constexpr ImU32 kTextColor         = IM_COL32(210, 210, 210, 255);
    constexpr ImU32 kDimTextColor      = IM_COL32(140, 140, 140, 255);
    constexpr ImU32 kGridColor         = IM_COL32(60, 60, 60, 255);
    constexpr ImU32 kOutsideColor      = IM_COL32(0, 0, 0, 70);      ///< duration より後ろの暗幕
    constexpr ImU32 kPlayheadColor     = IM_COL32(255, 90, 90, 255);
    constexpr ImU32 kSelectedKeyColor  = IM_COL32(255, 170, 40, 255);
    constexpr ImU32 kEventColor        = IM_COL32(120, 220, 160, 255);
    constexpr ImU32 kSelectRectColor   = IM_COL32(100, 150, 255, 50);
    constexpr ImU32 kSelectBorderColor = IM_COL32(100, 150, 255, 200);

    constexpr float kRulerSteps[] = { 0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
    constexpr float kMinRulerSpacing = 64.0f;  ///< 目盛りラベルどうしの最小間隔（px）
    constexpr float kMinGridSpacing  = 8.0f;   ///< これより細かいスナップ格子は描かない（px）

    void DrawDiamond(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, bool isSquare) {
      if (isSquare) {
        drawList->AddRectFilled(ImVec2(center.x - size * 0.8f, center.y - size * 0.8f), ImVec2(center.x + size * 0.8f, center.y + size * 0.8f), color);
        return;
      }
      const ImVec2 points[] = { { center.x, center.y - size }, { center.x + size, center.y }, { center.x, center.y + size }, { center.x - size, center.y } };
      drawList->AddConvexPolyFilled(points, 4, color);
      drawList->AddPolyline(points, 4, IM_COL32(20, 20, 20, 255), ImDrawFlags_Closed, 1.0f);
    }

    /// <summary>
    /// 値グラフで動かした値を、プロパティが取る値に丸める（Frame は 0 以上の整数、Visible は 0 か 1）
    /// </summary>
    float QuantizeValue(AnimProperty property, float value) {
      switch (property) {
      case AnimProperty::Frame:   return (std::max)(std::round(value), 0.0f);
      case AnimProperty::Visible: return value >= 0.5f ? 1.0f : 0.0f;
      default:                    return value;
      }
    }

  } // namespace

  void AnimationTimeline::Draw(AnimationClip& clip, float& playhead, float height) {
    PruneSelection(clip);
    const int trackCount = static_cast<int>(clip.GetTracks().size());
    selectedTrack_ = (std::min)(selectedTrack_, trackCount - 1);

    if (ImGui::BeginChild("Dopesheet", ImVec2(0.0f, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse)) {
      origin_ = ImGui::GetCursorScreenPos();
      width_  = ImGui::GetContentRegionAvail().x;

      if (isFitRequested_ && width_ > kLabelWidth) {
        const float span = (std::max)(clip.GetDuration(), 0.1f) * 1.15f;
        pixelsPerSecond_ = std::clamp((width_ - kLabelWidth) / span, kMinPixelsPerSecond, kMaxPixelsPerSecond);
        offset_          = 0.0f;
        isFitRequested_  = false;
      }

      // スクロール範囲を確保する（行は自前で描く）
      ImGui::Dummy(ImVec2(width_, kRulerHeight + kRowHeight * static_cast<float>(trackCount + 1)));

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      DrawRows(clip, drawList);
      DrawRuler(clip, drawList, playhead);

      if (operation_ == Operation::RectSelect) {
        drawList->AddRectFilled(dragStart_, dragCurrent_, kSelectRectColor);
        drawList->AddRect(dragStart_, dragCurrent_, kSelectBorderColor);
      }

      HandleInput(clip, playhead);
    }
    ImGui::EndChild();
  }

  void AnimationTimeline::DrawCurve(AnimationClip& clip, float& playhead) {
    if (!ImGui::BeginChild("Curve", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse)) {
      ImGui::EndChild();
      return;
    }

    std::vector<AnimTrack>& tracks = clip.GetTracks();
    if (selectedTrack_ < 0 || selectedTrack_ >= static_cast<int>(tracks.size()) || tracks[selectedTrack_].keys.empty()) {
      ImGui::TextDisabled("Select a track with keys to see its values over time.");
      ImGui::EndChild();
      return;
    }
    AnimTrack& track = tracks[selectedTrack_];

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasMin  = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x <= kLabelWidth || canvasSize.y < 20.0f) {
      ImGui::EndChild();
      return;
    }
    // ドープシートと同じ時間軸にするため、横方向は同じ原点からの距離で描く
    const float plotLeft  = canvasMin.x + kLabelWidth;
    const float plotRight = canvasMin.x + canvasSize.x;
    curveView_.originX = canvasMin.x;
    curveView_.top     = canvasMin.y + 4.0f;
    curveView_.bottom  = canvasMin.y + canvasSize.y - 4.0f;

    // 表示中の範囲の値を折れ線にする
    const int          sampleCount = (std::max)(static_cast<int>((plotRight - plotLeft) / 3.0f), 2);
    std::vector<float> samples(sampleCount);
    float              minValue = (std::numeric_limits<float>::max)();
    float              maxValue = std::numeric_limits<float>::lowest();
    for (int i = 0; i < sampleCount; ++i) {
      const float x = plotLeft + (plotRight - plotLeft) * static_cast<float>(i) / static_cast<float>(sampleCount - 1);
      samples[i] = AnimationClip::EvaluateTrack(track, CurveXToTime(x));
      minValue   = (std::min)(minValue, samples[i]);
      maxValue   = (std::max)(maxValue, samples[i]);
    }
    for (const AnimKey& key : track.keys) {
      minValue = (std::min)(minValue, key.value);
      maxValue = (std::max)(maxValue, key.value);
    }
    // Visible は 0 と 1 しか取らないので、ドラッグで切り替えてもキーが範囲の外へ出ないよう両方を入れておく
    if (track.property == AnimProperty::Visible) {
      minValue = (std::min)(minValue, 0.0f);
      maxValue = (std::max)(maxValue, 1.0f);
    }
    if (maxValue - minValue < 1e-4f) {
      minValue -= 0.5f;
      maxValue += 0.5f;
    }
    // ドラッグ中は縦の範囲を変えない（範囲の外へ動かしたときに 1px あたりの値が毎フレーム変わり、キーが暴れるため）
    if (operation_ != Operation::CurveMove) {
      const float padding = (maxValue - minValue) * 0.1f;
      curveView_.minValue = minValue - padding;
      curveView_.maxValue = maxValue + padding;
    }

    drawList->AddRectFilled(canvasMin, ImVec2(plotRight, canvasMin.y + canvasSize.y), kBackgroundColor);
    drawList->AddRectFilled(canvasMin, ImVec2(plotLeft, canvasMin.y + canvasSize.y), kLabelColor);
    drawList->AddText(ImVec2(canvasMin.x + 6.0f, canvasMin.y + 4.0f), kTextColor, TrackLabel(track).c_str());
    drawList->AddText(ImVec2(canvasMin.x + 6.0f, canvasMin.y + 22.0f), kDimTextColor, std::format("max {:.3g}", maxValue).c_str());
    drawList->AddText(ImVec2(canvasMin.x + 6.0f, curveView_.bottom - 16.0f), kDimTextColor, std::format("min {:.3g}", minValue).c_str());
    if (canvasSize.y > 110.0f) {
      drawList->AddText(ImVec2(canvasMin.x + 6.0f, canvasMin.y + 44.0f), kDimTextColor, "Drag a key: value / time\nShift+Drag: one axis\nDouble-click: add a key");
    }

    drawList->PushClipRect(ImVec2(plotLeft, canvasMin.y), ImVec2(plotRight, canvasMin.y + canvasSize.y), true);

    // 静止時の値（上乗せが効いていない高さ）
    const float restY = curveView_.ValueToY(GetAnimPropertyInfo(track.property).restValue);
    drawList->AddLine(ImVec2(plotLeft, restY), ImVec2(plotRight, restY), kGridColor);

    const ImU32 color = TrackColor(track.property);
    for (int i = 1; i < sampleCount; ++i) {
      const float x0 = plotLeft + (plotRight - plotLeft) * static_cast<float>(i - 1) / static_cast<float>(sampleCount - 1);
      const float x1 = plotLeft + (plotRight - plotLeft) * static_cast<float>(i) / static_cast<float>(sampleCount - 1);
      drawList->AddLine(ImVec2(x0, curveView_.ValueToY(samples[i - 1])), ImVec2(x1, curveView_.ValueToY(samples[i])), color, 2.0f);
    }
    const bool     isHovered = ImGui::IsWindowHovered();
    const uint32_t hovered   = isHovered ? HitTestCurve(track, ImGui::GetIO().MousePos) : 0;
    for (const AnimKey& key : track.keys) {
      const float radius = key.id == hovered ? 5.5f : 4.0f;
      drawList->AddCircleFilled(ImVec2(CurveTimeToX(key.time), curveView_.ValueToY(key.value)), radius, IsSelected(key.id) ? kSelectedKeyColor : color);
    }

    const float playheadX = CurveTimeToX(playhead);
    drawList->AddLine(ImVec2(playheadX, canvasMin.y), ImVec2(playheadX, canvasMin.y + canvasSize.y), kPlayheadColor, 1.5f);
    drawList->PopClipRect();

    HandleCurveInput(clip, track, playhead, isHovered);

    ImGui::EndChild();
  }

  void AnimationTimeline::PruneSelection(AnimationClip& clip) {
    std::erase_if(selection_, [&](uint32_t id) { return !FindTime(clip, id); });
  }

  void AnimationTimeline::SelectAll(const AnimationClip& clip) {
    selection_.clear();
    for (const AnimTrack& track : clip.GetTracks()) {
      for (const AnimKey& key : track.keys) {
        selection_.push_back(key.id);
      }
    }
    for (const AnimEvent& event : clip.GetEvents()) {
      selection_.push_back(event.id);
    }
  }

  float AnimationTimeline::Snap(float time) const {
    if (snapInterval_ <= 0.0f) {
      return time;
    }
    return std::round(time / snapInterval_) * snapInterval_;
  }

  std::string AnimationTimeline::TrackLabel(const AnimTrack& track) {
    return std::format("{}  {}", track.target.empty() ? "(Bound)" : track.target, GetAnimPropertyInfo(track.property).name);
  }

  bool AnimationTimeline::IsSelected(uint32_t id) const {
    return std::ranges::find(selection_, id) != selection_.end();
  }

  void AnimationTimeline::DrawRows(const AnimationClip& clip, ImDrawList* drawList) {
    const std::vector<AnimTrack>& tracks = clip.GetTracks();
    const float right      = origin_.x + width_;
    const float keysLeft   = origin_.x + kLabelWidth;
    const float rowsBottom = RowTop(static_cast<int>(tracks.size()) + 1);

    drawList->AddRectFilled(ImVec2(keysLeft, RowTop(0)), ImVec2(right, rowsBottom), kBackgroundColor);
    for (int row = 0; row <= static_cast<int>(tracks.size()); ++row) {
      const ImU32 color = row - 1 == selectedTrack_ ? kSelectedRowColor : (row % 2 == 0 ? kRowColor : kRowAltColor);
      drawList->AddRectFilled(ImVec2(keysLeft, RowTop(row)), ImVec2(right, RowTop(row) + kRowHeight), color);
    }

    drawList->PushClipRect(ImVec2(keysLeft, origin_.y), ImVec2(right, rowsBottom), true);

    // スナップ格子。細かすぎると見えないので目盛りの間隔に落とす
    const float gridStep = snapInterval_ * pixelsPerSecond_ >= kMinGridSpacing ? snapInterval_ : RulerStep();
    for (float time = std::floor(offset_ / gridStep) * gridStep; TimeToX(time) < right; time += gridStep) {
      drawList->AddLine(ImVec2(TimeToX(time), RowTop(0)), ImVec2(TimeToX(time), rowsBottom), kGridColor);
    }

    const float endX = TimeToX(clip.GetDuration());
    if (endX < right) {
      drawList->AddRectFilled(ImVec2((std::max)(endX, keysLeft), RowTop(0)), ImVec2(right, rowsBottom), kOutsideColor);
    }

    // イベント（旗）
    for (const AnimEvent& event : clip.GetEvents()) {
      const float x     = TimeToX(event.time);
      const float top   = RowTop(0) + 3.0f;
      const ImU32 color = IsSelected(event.id) ? kSelectedKeyColor : kEventColor;
      drawList->AddLine(ImVec2(x, top), ImVec2(x, top + kRowHeight - 6.0f), color, 2.0f);
      drawList->AddTriangleFilled(ImVec2(x, top), ImVec2(x + 9.0f, top + 4.0f), ImVec2(x, top + 8.0f), color);
      drawList->AddText(ImVec2(x + 11.0f, top), kDimTextColor, event.name.c_str());
    }

    // キー。区間は細い線でつなぎ、補間しない区間（Constant・Visible）は四角にする
    for (size_t i = 0; i < tracks.size(); ++i) {
      const AnimTrack& track   = tracks[i];
      const float      centerY = RowTop(static_cast<int>(i) + 1) + kRowHeight * 0.5f;
      const ImU32      color   = TrackColor(track.property);
      for (size_t k = 0; k + 1 < track.keys.size(); ++k) {
        drawList->AddLine(ImVec2(TimeToX(track.keys[k].time), centerY), ImVec2(TimeToX(track.keys[k + 1].time), centerY), (color & 0x00FFFFFF) | 0x80000000, 2.0f);
      }
      for (const AnimKey& key : track.keys) {
        const bool isSquare = GetAnimPropertyInfo(track.property).isStep || key.ease == EaseType::Constant;
        DrawDiamond(drawList, ImVec2(TimeToX(key.time), centerY), kKeySize, IsSelected(key.id) ? kSelectedKeyColor : color, isSquare);
      }
    }
    drawList->PopClipRect();

    // 見出しの列はキーの上に描く（左へスクロールしたキーを隠す）
    drawList->AddRectFilled(ImVec2(origin_.x, RowTop(0)), ImVec2(keysLeft, rowsBottom), kLabelColor);
    drawList->AddText(ImVec2(origin_.x + 6.0f, RowTop(0) + 4.0f), kEventColor, "Events");
    for (size_t i = 0; i < tracks.size(); ++i) {
      const float top = RowTop(static_cast<int>(i) + 1);
      if (static_cast<int>(i) == selectedTrack_) {
        drawList->AddRectFilled(ImVec2(origin_.x, top), ImVec2(keysLeft, top + kRowHeight), kSelectedRowColor);
      }
      drawList->AddRectFilled(ImVec2(origin_.x + 4.0f, top + 6.0f), ImVec2(origin_.x + 8.0f, top + kRowHeight - 6.0f), TrackColor(tracks[i].property));
      drawList->AddText(ImVec2(origin_.x + 14.0f, top + 4.0f), kTextColor, TrackLabel(tracks[i]).c_str());
    }
  }

  void AnimationTimeline::DrawRuler(const AnimationClip& clip, ImDrawList* drawList, float playhead) {
    const float right    = origin_.x + width_;
    const float keysLeft = origin_.x + kLabelWidth;
    const float bottom   = origin_.y + kRulerHeight;

    drawList->AddRectFilled(origin_, ImVec2(right, bottom), kLabelColor);
    drawList->AddText(ImVec2(origin_.x + 6.0f, origin_.y + 4.0f), kTextColor, std::format("{:.2f} / {:.2f} s", playhead, clip.GetDuration()).c_str());

    drawList->PushClipRect(ImVec2(keysLeft, origin_.y), ImVec2(right, origin_.y + 10000.0f), true);
    const float step = RulerStep();
    for (float time = std::floor(offset_ / step) * step; TimeToX(time) < right; time += step) {
      const float x = TimeToX(time);
      drawList->AddLine(ImVec2(x, bottom - 6.0f), ImVec2(x, bottom), kDimTextColor);
      drawList->AddText(ImVec2(x + 3.0f, origin_.y + 3.0f), kDimTextColor, std::format("{:.{}f}", time + 0.0f, step < 0.1f ? 2 : 1).c_str());
    }

    // 再生ヘッドは目盛りから最後の行まで通す
    const float x         = TimeToX(playhead);
    const float rowsBottom = RowTop(static_cast<int>(clip.GetTracks().size()) + 1);
    drawList->AddLine(ImVec2(x, origin_.y), ImVec2(x, rowsBottom), kPlayheadColor, 1.5f);
    drawList->AddTriangleFilled(ImVec2(x - 6.0f, origin_.y), ImVec2(x + 6.0f, origin_.y), ImVec2(x, origin_.y + 10.0f), kPlayheadColor);
    drawList->PopClipRect();
  }

  void AnimationTimeline::HandleInput(AnimationClip& clip, float& playhead) {
    const ImGuiIO& io         = ImGui::GetIO();
    const ImVec2   mouse      = io.MousePos;
    const bool     isHovered  = ImGui::IsWindowHovered();
    const int      trackCount = static_cast<int>(clip.GetTracks().size());
    const int      rowAtMouse = RowAt(mouse.y);
    const int      row        = rowAtMouse > trackCount ? -2 : rowAtMouse;  // 最後の行より下は範囲外
    const bool     isInKeys   = mouse.x > origin_.x + kLabelWidth;
    const float    duration   = clip.GetDuration();

    // 修飾キー無しのホイールは行の縦スクロール
    if (isHovered && io.MouseWheel != 0.0f && !io.KeyCtrl && !io.KeyShift) {
      ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseWheel * kRowHeight * 3.0f);
    }
    if (HandleViewInput(origin_.x, duration, isHovered)) {
      return;
    }

    // 見出しの右クリックでトラックのメニュー
    if (isHovered && !isInKeys && row >= 1 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      contextTrack_  = row - 1;
      selectedTrack_ = contextTrack_;
      ImGui::OpenPopup("TrackMenu");
    }
    if (ImGui::BeginPopup("TrackMenu")) {
      std::vector<AnimTrack>& tracks = clip.GetTracks();
      if (contextTrack_ >= 0 && contextTrack_ < static_cast<int>(tracks.size())) {
        ImGui::TextDisabled("%s", TrackLabel(tracks[contextTrack_]).c_str());
        if (ImGui::MenuItem("Select Keys")) {
          selection_.clear();
          for (const AnimKey& key : tracks[contextTrack_].keys) {
            selection_.push_back(key.id);
          }
        }
        if (ImGui::MenuItem("Delete Track")) {
          tracks.erase(tracks.begin() + contextTrack_);
          selectedTrack_ = -1;
          contextTrack_  = -1;
        }
      }
      ImGui::EndPopup();
    }

    // ダブルクリックは 1 回目のクリックで始めた範囲選択を取り消して追加する
    if (isHovered && isInKeys && row >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && HitTest(clip, row, mouse.x) == 0) {
      operation_ = Operation::None;
      AddAt(clip, row, Snap(std::clamp(XToTime(mouse.x), 0.0f, duration)));
      return;
    }

    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      if (row == -1 && isInKeys) {
        operation_ = Operation::Scrub;
      }
      else if (row >= 1 && !isInKeys) {
        selectedTrack_ = row - 1;
      }
      else if (row >= 0 && isInKeys) {
        if (row >= 1) {
          selectedTrack_ = row - 1;
        }
        if (const uint32_t hit = HitTest(clip, row, mouse.x); hit != 0) {
          SelectOnPress(hit, io.KeyCtrl);
          if (IsSelected(hit)) {
            BeginMove(clip, hit);
            playhead = dragAnchorTime_;
          }
        }
        else {
          if (!io.KeyCtrl) {
            selection_.clear();
          }
          operation_   = Operation::RectSelect;
          dragStart_   = mouse;
          dragCurrent_ = mouse;
        }
      }
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      switch (operation_) {
      case Operation::Scrub:
        playhead = std::clamp(XToTime(mouse.x), 0.0f, duration);
        break;
      case Operation::Move:
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          pendingSingle_ = 0;
          UpdateMove(clip, playhead);
        }
        break;
      case Operation::RectSelect:
        dragCurrent_ = mouse;
        break;
      default:
        break;
      }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      FinishDrag(clip);
    }
  }

  void AnimationTimeline::HandleCurveInput(AnimationClip& clip, AnimTrack& track, float& playhead, bool isHovered) {
    const ImGuiIO& io       = ImGui::GetIO();
    const ImVec2   mouse    = io.MousePos;
    const float    duration = clip.GetDuration();
    const bool     isInPlot = isHovered && mouse.x > curveView_.originX + kLabelWidth;

    if (HandleViewInput(curveView_.originX, duration, isHovered)) {
      return;
    }

    // ダブルクリックは、その時刻とカーソルの高さの値でキーを足す
    if (isInPlot && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && HitTestCurve(track, mouse) == 0) {
      AddAt(clip, selectedTrack_ + 1, Snap(std::clamp(CurveXToTime(mouse.x), 0.0f, duration)));
      if (AnimKey* key = clip.FindKey(selection_.front())) {
        key->value = QuantizeValue(track.property, curveView_.YToValue(mouse.y));
      }
      return;
    }

    if (isInPlot && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      if (const uint32_t hit = HitTestCurve(track, mouse); hit != 0) {
        SelectOnPress(hit, io.KeyCtrl);
        if (IsSelected(hit)) {
          BeginCurveMove(track, hit);
          playhead = dragAnchorTime_;
        }
      }
      else if (!io.KeyCtrl) {
        selection_.clear();
      }
    }

    if (operation_ == Operation::CurveMove && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        pendingSingle_ = 0;
        UpdateCurveMove(clip, track.property, playhead);
      }
      if (const AnimKey* key = clip.FindKey(curveAnchorId_)) {
        ImGui::SetTooltip("%.3f s  /  %.3f", key->time, key->value);
      }
    }

    // 先に呼ばれる HandleInput が終えていれば何もしない（ドープシートが描かれないフレームのため）
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      FinishDrag(clip);
    }
  }

  bool AnimationTimeline::HandleViewInput(float originX, float duration, bool isHovered) {
    const ImGuiIO& io    = ImGui::GetIO();
    const ImVec2   mouse = io.MousePos;

    // Ctrl+ホイールでカーソル位置を中心に拡大縮小、Shift+ホイールで横にスクロール
    if (isHovered && io.MouseWheel != 0.0f && (io.KeyCtrl || io.KeyShift)) {
      if (io.KeyCtrl) {
        const float keysLeft    = originX + kLabelWidth;
        const float timeAtMouse = (mouse.x - keysLeft) / pixelsPerSecond_ + offset_;
        pixelsPerSecond_ = std::clamp(pixelsPerSecond_ * (io.MouseWheel > 0.0f ? 1.2f : 1.0f / 1.2f), kMinPixelsPerSecond, kMaxPixelsPerSecond);
        offset_          = timeAtMouse - (mouse.x - keysLeft) / pixelsPerSecond_;
      }
      else {
        offset_ -= io.MouseWheel * 60.0f / pixelsPerSecond_;
      }
      ClampOffset(duration);
    }

    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      operation_      = Operation::Pan;
      dragStart_      = mouse;
      panStartOffset_ = offset_;
    }
    if (operation_ != Operation::Pan) {
      return false;
    }
    offset_ = panStartOffset_ - (mouse.x - dragStart_.x) / pixelsPerSecond_;
    ClampOffset(duration);
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
      operation_ = Operation::None;
    }
    return true;
  }

  void AnimationTimeline::SelectOnPress(uint32_t id, bool isToggle) {
    if (isToggle) {
      if (IsSelected(id)) {
        std::erase(selection_, id);
      }
      else {
        selection_.push_back(id);
      }
    }
    else if (IsSelected(id)) {
      // 複数選択のまま動かせるよう選択を保つ。動かさずに離したらこれだけを選ぶ
      pendingSingle_ = id;
    }
    else {
      selection_ = { id };
    }
  }

  void AnimationTimeline::FinishDrag(const AnimationClip& clip) {
    if (operation_ == Operation::RectSelect) {
      FinishRectSelect(clip);
    }
    if (pendingSingle_ != 0) {
      selection_ = { pendingSingle_ };
    }
    operation_     = Operation::None;
    pendingSingle_ = 0;
    dragItems_.clear();
  }

  uint32_t AnimationTimeline::HitTest(const AnimationClip& clip, int row, float x) const {
    const float radius = kKeySize + 2.0f;
    if (row == 0) {
      const std::vector<AnimEvent>& events = clip.GetEvents();
      for (auto it = events.rbegin(); it != events.rend(); ++it) {
        // 旗は線の右側に出るので、右へ少し広めに取る
        const float eventX = TimeToX(it->time);
        if (x >= eventX - radius && x <= eventX + radius + 4.0f) {
          return it->id;
        }
      }
      return 0;
    }
    const std::vector<AnimTrack>& tracks = clip.GetTracks();
    if (row < 1 || row > static_cast<int>(tracks.size())) {
      return 0;
    }
    const std::vector<AnimKey>& keys = tracks[row - 1].keys;
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
      if (std::abs(TimeToX(it->time) - x) <= radius) {
        return it->id;
      }
    }
    return 0;
  }

  uint32_t AnimationTimeline::HitTestCurve(const AnimTrack& track, ImVec2 position) const {
    constexpr float kRadius = 6.0f;
    for (auto it = track.keys.rbegin(); it != track.keys.rend(); ++it) {
      const float dx = CurveTimeToX(it->time) - position.x;
      const float dy = curveView_.ValueToY(it->value) - position.y;
      if (dx * dx + dy * dy <= kRadius * kRadius) {
        return it->id;
      }
    }
    return 0;
  }

  void AnimationTimeline::AddAt(AnimationClip& clip, int row, float time) {
    const uint32_t id = clip.IssueId();
    if (row == 0) {
      clip.GetEvents().push_back(AnimEvent{ .time = time, .name = "Event", .id = id });
    }
    else {
      AnimTrack& track = clip.GetTracks()[row - 1];
      track.keys.push_back(AnimKey{ .time = time, .value = AnimationClip::EvaluateTrack(track, time), .ease = GetAnimPropertyInfo(track.property).defaultEase, .id = id });
      selectedTrack_ = row - 1;
    }
    clip.SortByTime();
    selection_ = { id };
  }

  void AnimationTimeline::BeginMove(AnimationClip& clip, uint32_t anchorId) {
    dragItems_.clear();
    for (uint32_t id : selection_) {
      if (const float* time = FindTime(clip, id)) {
        dragItems_.push_back({ id, *time });
      }
    }
    const float* anchorTime = FindTime(clip, anchorId);
    dragAnchorTime_ = anchorTime ? *anchorTime : 0.0f;
    dragStart_      = ImGui::GetIO().MousePos;
    operation_      = Operation::Move;
  }

  void AnimationTimeline::UpdateMove(AnimationClip& clip, float& playhead) {
    if (dragItems_.empty()) {
      return;
    }
    // 掴んだものの時刻をスナップし、全体を同じだけずらす。どれも 0～duration に収まる範囲に抑える
    float minStart = (std::numeric_limits<float>::max)();
    float maxStart = std::numeric_limits<float>::lowest();
    for (const DragItem& item : dragItems_) {
      minStart = (std::min)(minStart, item.startTime);
      maxStart = (std::max)(maxStart, item.startTime);
    }
    const float anchorTime = Snap(dragAnchorTime_ + (ImGui::GetIO().MousePos.x - dragStart_.x) / pixelsPerSecond_);
    const float delta      = std::clamp(anchorTime - dragAnchorTime_, -minStart, (std::max)(clip.GetDuration() - maxStart, -minStart));

    for (const DragItem& item : dragItems_) {
      if (float* time = FindTime(clip, item.id)) {
        *time = item.startTime + delta;
      }
    }
    clip.SortByTime();
    playhead = dragAnchorTime_ + delta;
  }

  void AnimationTimeline::BeginCurveMove(const AnimTrack& track, uint32_t anchorId) {
    // 選択はほかのトラックのキーやイベントも含むので、表示中のトラックのキーだけを動かす
    dragItems_.clear();
    for (const AnimKey& key : track.keys) {
      if (IsSelected(key.id)) {
        dragItems_.push_back({ key.id, key.time, key.value });
      }
      if (key.id == anchorId) {
        dragAnchorTime_ = key.time;
      }
    }
    curveAnchorId_ = anchorId;
    dragStart_     = ImGui::GetIO().MousePos;
    operation_     = Operation::CurveMove;
  }

  void AnimationTimeline::UpdateCurveMove(AnimationClip& clip, AnimProperty property, float& playhead) {
    // 横に少し動くまでは時刻を変えない（縦だけ動かしたつもりでも、時刻がスナップ間隔へ丸め直されてしまうため）
    constexpr float kTimeDeadZone = 4.0f;  // px

    const ImGuiIO& io = ImGui::GetIO();
    const float    dx = io.MousePos.x - dragStart_.x;
    const float    dy = io.MousePos.y - dragStart_.y;
    const bool     movesTime  = std::abs(dx) >= kTimeDeadZone && (!io.KeyShift || std::abs(dx) >= std::abs(dy));
    const bool     movesValue = !io.KeyShift || std::abs(dy) > std::abs(dx);

    // 縦の範囲はドラッグ中は変わらないので、始点との差がそのまま値の差になる
    const float valueDelta = movesValue ? curveView_.YToValue(io.MousePos.y) - curveView_.YToValue(dragStart_.y) : 0.0f;
    for (const DragItem& item : dragItems_) {
      if (AnimKey* key = clip.FindKey(item.id)) {
        key->value = QuantizeValue(property, item.startValue + valueDelta);
        if (!movesTime) {
          key->time = item.startTime;
        }
      }
    }
    if (movesTime) {
      UpdateMove(clip, playhead);
    }
    else {
      clip.SortByTime();
      playhead = dragAnchorTime_;
    }
  }

  void AnimationTimeline::FinishRectSelect(const AnimationClip& clip) {
    const ImVec2 min = { (std::min)(dragStart_.x, dragCurrent_.x), (std::min)(dragStart_.y, dragCurrent_.y) };
    const ImVec2 max = { (std::max)(dragStart_.x, dragCurrent_.x), (std::max)(dragStart_.y, dragCurrent_.y) };
    auto select = [&](uint32_t id, float time, int row) {
      const float x = TimeToX(time);
      const float y = RowTop(row) + kRowHeight * 0.5f;
      if (x >= min.x && x <= max.x && y >= min.y && y <= max.y && !IsSelected(id)) {
        selection_.push_back(id);
      }
    };
    for (const AnimEvent& event : clip.GetEvents()) {
      select(event.id, event.time, 0);
    }
    const std::vector<AnimTrack>& tracks = clip.GetTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
      for (const AnimKey& key : tracks[i].keys) {
        select(key.id, key.time, static_cast<int>(i) + 1);
      }
    }
  }

  int AnimationTimeline::RowAt(float screenY) const {
    if (screenY < origin_.y) {
      return -2;
    }
    if (screenY < origin_.y + kRulerHeight) {
      return -1;
    }
    return static_cast<int>((screenY - origin_.y - kRulerHeight) / kRowHeight);
  }

  float AnimationTimeline::RulerStep() const {
    for (float step : kRulerSteps) {
      if (step * pixelsPerSecond_ >= kMinRulerSpacing) {
        return step;
      }
    }
    return kRulerSteps[std::size(kRulerSteps) - 1];
  }

  void AnimationTimeline::ClampOffset(float duration) {
    const float visibleSpan = (width_ - kLabelWidth) / pixelsPerSecond_;
    offset_ = std::clamp(offset_, -0.1f, (std::max)(duration - visibleSpan * 0.5f, -0.1f));
  }

  float* AnimationTimeline::FindTime(AnimationClip& clip, uint32_t id) {
    if (AnimKey* key = clip.FindKey(id)) {
      return &key->time;
    }
    if (AnimEvent* event = clip.FindEvent(id)) {
      return &event->time;
    }
    return nullptr;
  }

  ImU32 AnimationTimeline::TrackColor(AnimProperty property) {
    switch (property) {
    case AnimProperty::TranslateX: return IM_COL32(235, 95, 95, 255);
    case AnimProperty::TranslateY: return IM_COL32(110, 210, 110, 255);
    case AnimProperty::TranslateZ: return IM_COL32(100, 140, 240, 255);
    case AnimProperty::RotateX:
    case AnimProperty::RotateY:
    case AnimProperty::RotateZ:    return IM_COL32(240, 200, 90, 255);
    case AnimProperty::ScaleX:
    case AnimProperty::ScaleY:
    case AnimProperty::ScaleZ:     return IM_COL32(190, 130, 240, 255);
    case AnimProperty::ColorR:     return IM_COL32(255, 120, 120, 255);
    case AnimProperty::ColorG:     return IM_COL32(120, 255, 120, 255);
    case AnimProperty::ColorB:     return IM_COL32(120, 160, 255, 255);
    case AnimProperty::ColorA:     return IM_COL32(220, 220, 220, 255);
    case AnimProperty::Frame:      return IM_COL32(250, 230, 120, 255);
    case AnimProperty::Visible:    return IM_COL32(110, 220, 230, 255);
    default:                       return IM_COL32(160, 160, 160, 255);
    }
  }

} // namespace Tako

#endif // _DEBUG
