#ifdef _DEBUG

#include "CameraAnimationTimeline.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace Tako {

  CameraAnimationTimeline::CameraAnimationTimeline() {
    // 初期は SUMMARY トラックのみ表示
    for (int i = 0; i < static_cast<int>(TrackType::COUNT); ++i) {
      trackVisible_[i] = (i == 0);
    }
  }

  CameraAnimationTimeline::~CameraAnimationTimeline() {
  }

  void CameraAnimationTimeline::Initialize(CameraAnimation* animation) {
    animation_ = animation;
    selectedKeyframes_.clear();
    hoveredKeyframe_ = -1;
  }

  void CameraAnimationTimeline::Draw(float& playhead) {
    if (!animation_) return;

    // 削除・Reload・Undo で無くなったキーフレームを選択から外す
    std::erase_if(selectedKeyframes_, [this](uint32_t id) { return animation_->FindKeyframeIndex(id) < 0; });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    if (contentSize.x < 100 || contentSize.y < 100) {
      ImGui::PopStyleVar(2);
      return;
    }

    if (ImGui::BeginChild("Timeline", ImVec2(contentSize.x, timelineHeight_), true,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      ImVec2 canvasPos = ImGui::GetCursorScreenPos();
      ImVec2 canvasSize = ImGui::GetContentRegionAvail();

      drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(40, 40, 40, 255));

      DrawGrid();

      DrawTimeRuler();

      float yPos = canvasPos.y + rulerHeight_;
      for (int i = 0; i < static_cast<int>(TrackType::COUNT); ++i) {
        if (trackVisible_[i]) {
          DrawTrack(static_cast<TrackType>(i), yPos);
          yPos += trackHeight_;
        }
      }

      DrawPlayhead(playhead);

      if (isRectSelecting_) {
        DrawSelectionRect();
      }

      HandleMouseInput(playhead);
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
  }

  void CameraAnimationTimeline::Update(float deltaTime) {
    // ホバー中のみ拍動アニメの位相を進める
    if (hoveredKeyframe_ >= 0) {
      hoverAnimTime_ += deltaTime * 3.0f;
    }
    else {
      hoverAnimTime_ = 0.0f;
    }
  }

  void CameraAnimationTimeline::ClearSelection() {
    selectedKeyframes_.clear();
  }

  void CameraAnimationTimeline::SelectAll() {
    selectedKeyframes_.clear();
    for (size_t i = 0; i < animation_->GetKeyframeCount(); ++i) {
      selectedKeyframes_.push_back(animation_->GetKeyframe(i).id);
    }
  }

  bool CameraAnimationTimeline::IsSelected(uint32_t id) const {
    return std::ranges::find(selectedKeyframes_, id) != selectedKeyframes_.end();
  }

  void CameraAnimationTimeline::SetTrackVisible(TrackType track, bool visible) {
    int index = static_cast<int>(track);
    if (index >= 0 && index < static_cast<int>(TrackType::COUNT)) {
      trackVisible_[index] = visible;
    }
  }

  void CameraAnimationTimeline::DrawTimeRuler() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    drawList->AddRectFilled(
      ImVec2(canvasPos.x + trackLabelWidth_, canvasPos.y),
      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + rulerHeight_),
      IM_COL32(50, 50, 50, 255));

    float duration = animation_->GetDuration();
    float timeStep = 1.0f / zoom_;

    // ズーム量から見やすい目盛り間隔へ丸める
    if (timeStep < 0.1f) timeStep = 0.1f;
    else if (timeStep < 0.5f) timeStep = 0.5f;
    else if (timeStep < 1.0f) timeStep = 1.0f;
    else if (timeStep < 5.0f) timeStep = 5.0f;
    else timeStep = 10.0f;

    for (float time = 0; time <= duration + timeStep; time += timeStep) {
      float x = TimeToScreenX(time);
      if (x < trackLabelWidth_ || x > canvasSize.x) continue;

      // 主目盛り
      drawList->AddLine(
        ImVec2(canvasPos.x + x, canvasPos.y + rulerHeight_ - 10),
        ImVec2(canvasPos.x + x, canvasPos.y + rulerHeight_),
        IM_COL32(200, 200, 200, 255));

      std::string label = std::format("{:.1f}", time);
      drawList->AddText(
        ImVec2(canvasPos.x + x - 10, canvasPos.y + 2),
        IM_COL32(200, 200, 200, 255),
        label.c_str());

      // 副目盛り（0.1秒刻み）
      if (timeStep >= 1.0f) {
        for (float subTime = time + 0.1f; subTime < time + timeStep && subTime <= duration; subTime += 0.1f) {
          float subX = TimeToScreenX(subTime);
          if (subX < trackLabelWidth_ || subX > canvasSize.x) continue;

          drawList->AddLine(
            ImVec2(canvasPos.x + subX, canvasPos.y + rulerHeight_ - 5),
            ImVec2(canvasPos.x + subX, canvasPos.y + rulerHeight_),
            IM_COL32(100, 100, 100, 255));
        }
      }
    }
  }

  void CameraAnimationTimeline::DrawGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // 垂直線（スナップ間隔ごと）
    float timeStep = gridSnapInterval_;
    float duration = animation_->GetDuration();

    for (float time = 0; time <= duration + timeStep; time += timeStep) {
      float x = TimeToScreenX(time);
      if (x < trackLabelWidth_ || x > canvasSize.x) continue;

      drawList->AddLine(
        ImVec2(canvasPos.x + x, canvasPos.y + rulerHeight_),
        ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
        gridColor_, 1.0f);
    }

    // 水平線（トラック境界）
    float yPos = canvasPos.y + rulerHeight_;
    for (int i = 0; i < static_cast<int>(TrackType::COUNT); ++i) {
      if (trackVisible_[i]) {
        drawList->AddLine(
          ImVec2(canvasPos.x + trackLabelWidth_, yPos),
          ImVec2(canvasPos.x + canvasSize.x, yPos),
          gridColor_, 1.0f);
        yPos += trackHeight_;
      }
    }
  }

  void CameraAnimationTimeline::DrawPlayhead(float playhead) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    float x = TimeToScreenX(playhead);

    if (x >= trackLabelWidth_ && x <= canvasSize.x) {
      drawList->AddLine(
        ImVec2(canvasPos.x + x, canvasPos.y),
        ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
        playheadColor_, 2.0f);

      // 上部のハンドル（三角形）
      ImVec2 handlePoints[3] = {
        ImVec2(canvasPos.x + x - 5, canvasPos.y),
        ImVec2(canvasPos.x + x + 5, canvasPos.y),
        ImVec2(canvasPos.x + x, canvasPos.y + 10)
      };
      drawList->AddTriangleFilled(handlePoints[0], handlePoints[1], handlePoints[2], playheadColor_);
    }
  }

  void CameraAnimationTimeline::DrawTrack(TrackType trackType, float yPos) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    ImVec2 labelPos = ImVec2(canvasPos.x + 5, yPos + 5);
    drawList->AddText(labelPos, IM_COL32(200, 200, 200, 255), GetTrackName(trackType));

    // ラベル背景
    drawList->AddRectFilled(
      ImVec2(canvasPos.x, yPos),
      ImVec2(canvasPos.x + trackLabelWidth_, yPos + trackHeight_),
      IM_COL32(60, 60, 60, 255));

    // トラック背景
    drawList->AddRectFilled(
      ImVec2(canvasPos.x + trackLabelWidth_, yPos),
      ImVec2(canvasPos.x + canvasSize.x, yPos + trackHeight_),
      IM_COL32(45, 45, 45, 255));

    for (size_t i = 0; i < animation_->GetKeyframeCount(); ++i) {
      const CameraKeyframe& kf = animation_->GetKeyframe(i);
      float x = TimeToScreenX(kf.time);

      if (x < trackLabelWidth_ || x > canvasSize.x) continue;

      bool isSelected = IsSelected(kf.id);
      bool isHovered = (hoveredKeyframe_ == static_cast<int>(i) && hoveredTrack_ == trackType);

      if (trackType == TrackType::SUMMARY) {
        DrawKeyframe(x, yPos + trackHeight_ / 2, isSelected, isHovered);
      }
      else {
        // TODO: 個別トラックは該当プロパティが変化したキーフレームのみ表示する
        DrawKeyframe(x, yPos + trackHeight_ / 2, isSelected, isHovered);
      }
    }
  }

  void CameraAnimationTimeline::DrawKeyframe(float xPos, float yPos, bool isSelected, bool isHovered) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // ホバーで拍動、選択でさらに拡大
    float scale = 1.0f;
    if (isHovered) {
      scale = 1.0f + 0.2f * std::sin(hoverAnimTime_);
    }
    if (isSelected) {
      scale *= 1.1f;
    }

    float size = keyframeSize_ * scale;

    ImU32 color = IM_COL32(150, 150, 255, 255);
    if (isSelected) {
      color = selectedColor_;
    }
    else if (isHovered) {
      color = hoveredColor_;
    }

    ImVec2 center = ImVec2(canvasPos.x + xPos, yPos);

    switch (keyframeStyle_) {
    case KeyframeStyle::DIAMOND: {
      ImVec2 points[4] = {
        ImVec2(center.x, center.y - size),      // 上
        ImVec2(center.x + size, center.y),      // 右
        ImVec2(center.x, center.y + size),      // 下
        ImVec2(center.x - size, center.y)       // 左
      };
      drawList->AddConvexPolyFilled(points, 4, color);
      drawList->AddPolyline(points, 4, IM_COL32(255, 255, 255, 200), ImDrawFlags_Closed, 2.0f);
      break;
    }
    case KeyframeStyle::CIRCLE:
      drawList->AddCircleFilled(center, size, color);
      drawList->AddCircle(center, size, IM_COL32(255, 255, 255, 200), 0, 2.0f);
      break;

    case KeyframeStyle::SQUARE:
      drawList->AddRectFilled(
        ImVec2(center.x - size, center.y - size),
        ImVec2(center.x + size, center.y + size),
        color);
      drawList->AddRect(
        ImVec2(center.x - size, center.y - size),
        ImVec2(center.x + size, center.y + size),
        IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);
      break;

    case KeyframeStyle::TRIANGLE: {
      ImVec2 points[3] = {
        ImVec2(center.x, center.y - size),
        ImVec2(center.x + size, center.y + size),
        ImVec2(center.x - size, center.y + size)
      };
      drawList->AddTriangleFilled(points[0], points[1], points[2], color);
      drawList->AddTriangle(points[0], points[1], points[2],
        IM_COL32(255, 255, 255, 200), 2.0f);
      break;
    }
    }
  }

  void CameraAnimationTimeline::DrawSelectionRect() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 fillColor = IM_COL32(100, 150, 255, 50);
    ImU32 borderColor = IM_COL32(100, 150, 255, 200);

    drawList->AddRectFilled(dragStartPos_, dragCurrentPos_, fillColor);
    drawList->AddRect(dragStartPos_, dragCurrentPos_, borderColor);
  }

  void CameraAnimationTimeline::HandleMouseInput(float& playhead) {
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // 操作は見えている部分の上でだけ始める（親パネルからはみ出した部分や、上に重なったウィンドウのクリックを拾わない）。始めた操作は外に出ても離すまで続ける
    const bool isOperating = isScrubbing_ || isDragging_ || isRectSelecting_ || isPanning_;
    if (!isOperating && !ImGui::IsWindowHovered()) {
      return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      float relX = mousePos.x - canvasPos.x;
      float relY = mousePos.y - canvasPos.y;

      // ルーラー上クリックでスクラブ開始
      if (relY < rulerHeight_) {
        isScrubbing_ = true;
        playhead = std::clamp(ScreenXToTime(relX), 0.0f, animation_->GetDuration());
      }
      else if (relX > trackLabelWidth_) {
        // クリック位置のトラックを判定
        float trackY = rulerHeight_;
        for (int i = 0; i < static_cast<int>(TrackType::COUNT); ++i) {
          if (trackVisible_[i]) {
            if (relY >= trackY && relY < trackY + trackHeight_) {
              hoveredTrack_ = static_cast<TrackType>(i);
              break;
            }
            trackY += trackHeight_;
          }
        }

        int hitIndex = HitTestKeyframe(relX, relY, hoveredTrack_);

        if (hitIndex >= 0) {
          const uint32_t hitId = animation_->GetKeyframe(hitIndex).id;

          if (ImGui::GetIO().KeyCtrl) {
            // Ctrl+クリック：トグル選択
            if (IsSelected(hitId)) {
              std::erase(selectedKeyframes_, hitId);
            }
            else {
              selectedKeyframes_.push_back(hitId);
            }
          }
          else if (ImGui::GetIO().KeyShift && !selectedKeyframes_.empty()) {
            // Shift+クリック：直前の選択から範囲選択
            int lastSelected = animation_->FindKeyframeIndex(selectedKeyframes_.back());
            int start = std::min<int>(lastSelected, hitIndex);
            int end = std::max<int>(lastSelected, hitIndex);
            for (int i = start; i <= end; ++i) {
              const uint32_t id = animation_->GetKeyframe(i).id;
              if (!IsSelected(id)) {
                selectedKeyframes_.push_back(id);
              }
            }
          }
          else if (IsSelected(hitId)) {
            // 選択中のものを押したら、複数選択のままドラッグできるよう選択を保つ。動かさずに離したらこれだけを選ぶ
            pendingSingleSelectId_ = hitId;
          }
          else {
            // 通常クリック：単一選択
            selectedKeyframes_.clear();
            selectedKeyframes_.push_back(hitId);
          }

          // クリックしたキーフレームの時刻へ再生ヘッドを移す
          playhead = animation_->GetKeyframe(hitIndex).time;
          dragAnchorId_ = hitId;

          isDragging_ = true;
          dragStartPos_ = mousePos;
          dragStartTimes_.clear();
          for (uint32_t id : selectedKeyframes_) {
            dragStartTimes_.emplace_back(id, animation_->GetKeyframe(animation_->FindKeyframeIndex(id)).time);
          }
        }
        else {
          // 空白クリックで矩形選択開始
          if (!ImGui::GetIO().KeyCtrl) {
            selectedKeyframes_.clear();
          }

          isRectSelecting_ = true;
          dragStartPos_ = mousePos;
          dragCurrentPos_ = mousePos;
        }
      }
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      if (isScrubbing_) {
        float relX = mousePos.x - canvasPos.x;
        playhead = std::clamp(ScreenXToTime(relX), 0.0f, animation_->GetDuration());
      }
      else if (isDragging_) {
        pendingSingleSelectId_ = 0;
        ProcessKeyframeDrag(playhead);
      }
      else if (isRectSelecting_) {
        dragCurrentPos_ = mousePos;
      }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      if (isRectSelecting_) {
        ProcessRectSelection();
      }
      if (pendingSingleSelectId_ != 0) {
        selectedKeyframes_ = { pendingSingleSelectId_ };
      }

      isScrubbing_ = false;
      isDragging_ = false;
      isRectSelecting_ = false;
      dragStartTimes_.clear();
      dragAnchorId_ = 0;
      pendingSingleSelectId_ = 0;
    }

    // ホバー検出（ドラッグ/矩形選択中は除く）
    if (!isDragging_ && !isRectSelecting_) {
      float relX = mousePos.x - canvasPos.x;
      float relY = mousePos.y - canvasPos.y;

      float trackY = rulerHeight_;
      hoveredTrack_ = TrackType::SUMMARY;
      for (int i = 0; i < static_cast<int>(TrackType::COUNT); ++i) {
        if (trackVisible_[i]) {
          if (relY >= trackY && relY < trackY + trackHeight_) {
            hoveredTrack_ = static_cast<TrackType>(i);
            break;
          }
          trackY += trackHeight_;
        }
      }

      hoveredKeyframe_ = HitTestKeyframe(relX, relY, hoveredTrack_);
    }

    // 中ボタンドラッグでパン
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      isPanning_ = true;
      dragStartOffset_ = offset_;
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) && isPanning_) {
      ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
      offset_ = dragStartOffset_ - delta.x / (100.0f * zoom_);
      ClampOffset();
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
      isPanning_ = false;
    }

    if (ImGui::GetIO().MouseWheel != 0) {
      if (ImGui::GetIO().KeyCtrl) {
        // Ctrl+ホイール：マウス位置を中心にズーム
        float zoomDelta = ImGui::GetIO().MouseWheel > 0 ? 1.2f : 0.8f;
        float newZoom = zoom_ * zoomDelta;

        newZoom = std::max<float>(minZoom_, std::min<float>(maxZoom_, newZoom));

        // ズーム前後でマウス下の時刻が動かないよう offset を補正
        float mouseTime = ScreenXToTime(mousePos.x - canvasPos.x);
        zoom_ = newZoom;
        float newMouseTime = ScreenXToTime(mousePos.x - canvasPos.x);
        offset_ += (newMouseTime - mouseTime);
        ClampOffset();
      }
      else if (ImGui::GetIO().KeyShift) {
        // Shift+ホイール：横スクロール
        offset_ -= ImGui::GetIO().MouseWheel * 0.5f / zoom_;
        ClampOffset();
      }
    }
  }

  float CameraAnimationTimeline::TimeToScreenX(float time) const {
    return trackLabelWidth_ + (time - offset_) * 100.0f * zoom_;
  }

  float CameraAnimationTimeline::ScreenXToTime(float x) const {
    return ((x - trackLabelWidth_) / (100.0f * zoom_)) + offset_;
  }

  int CameraAnimationTimeline::HitTestKeyframe(float x, float y, TrackType trackType) const {
    // 対象トラックの Y 範囲を算出
    float trackY = rulerHeight_;
    for (int i = 0; i < static_cast<int>(trackType); ++i) {
      if (trackVisible_[i]) {
        trackY += trackHeight_;
      }
    }

    if (y < trackY || y > trackY + trackHeight_) {
      return -1;
    }

    for (size_t i = 0; i < animation_->GetKeyframeCount(); ++i) {
      const CameraKeyframe& kf = animation_->GetKeyframe(i);
      float kfX = TimeToScreenX(kf.time);
      float kfY = trackY + trackHeight_ / 2;

      float dist = std::sqrt((x - kfX) * (x - kfX) + (y - kfY) * (y - kfY));
      if (dist <= keyframeSize_) {
        return static_cast<int>(i);
      }
    }

    return -1;
  }

  void CameraAnimationTimeline::ProcessRectSelection() {
    float left = std::min<float>(dragStartPos_.x, dragCurrentPos_.x);
    float right = std::max<float>(dragStartPos_.x, dragCurrentPos_.x);
    float top = std::min<float>(dragStartPos_.y, dragCurrentPos_.y);
    float bottom = std::max<float>(dragStartPos_.y, dragCurrentPos_.y);

    for (size_t i = 0; i < animation_->GetKeyframeCount(); ++i) {
      const CameraKeyframe& kf = animation_->GetKeyframe(i);
      float kfX = TimeToScreenX(kf.time);

      // 表示中の各トラック上の位置を矩形と判定
      float trackY = rulerHeight_;
      for (int t = 0; t < static_cast<int>(TrackType::COUNT); ++t) {
        if (trackVisible_[t]) {
          float kfY = trackY + trackHeight_ / 2;

          if (kfX >= left && kfX <= right && kfY >= top && kfY <= bottom) {
            if (!IsSelected(kf.id)) {
              selectedKeyframes_.push_back(kf.id);
            }
            break;
          }
          trackY += trackHeight_;
        }
      }
    }
  }

  void CameraAnimationTimeline::ProcessKeyframeDrag(float& playhead) {
    ImVec2 mousePos = ImGui::GetMousePos();
    float deltaX = mousePos.x - dragStartPos_.x;
    float deltaTime = deltaX / (100.0f * zoom_);

    // 動かすたびに時刻順へ並び替わって添字が変わるので、毎回 id から引き直す
    for (const auto& [id, startTime] : dragStartTimes_) {
      const int idx = animation_->FindKeyframeIndex(id);
      if (idx < 0) {
        continue;
      }

      CameraKeyframe kf = animation_->GetKeyframe(idx);
      float newTime = startTime + deltaTime;

      if (enableGridSnap_) {
        newTime = SnapToGrid(newTime);
      }

      newTime = std::max<float>(0.0f, std::min<float>(newTime, animation_->GetDuration()));

      kf.time = newTime;
      animation_->EditKeyframe(idx, kf);

      if (id == dragAnchorId_) {
        playhead = newTime;
      }
    }
  }

  float CameraAnimationTimeline::SnapToGrid(float time) const {
    if (!enableGridSnap_) return time;
    return std::round(time / gridSnapInterval_) * gridSnapInterval_;
  }

  const char* CameraAnimationTimeline::GetTrackName(TrackType track) const {
    switch (track) {
    case TrackType::SUMMARY: return "Summary";
    case TrackType::POSITION_X: return "Pos X";
    case TrackType::POSITION_Y: return "Pos Y";
    case TrackType::POSITION_Z: return "Pos Z";
    case TrackType::ROTATION_X: return "Rot X";
    case TrackType::ROTATION_Y: return "Rot Y";
    case TrackType::ROTATION_Z: return "Rot Z";
    case TrackType::FOV: return "FOV";
    default: return "Unknown";
    }
  }

  ImU32 CameraAnimationTimeline::GetTrackColor(TrackType track) const {
    switch (track) {
    case TrackType::SUMMARY: return IM_COL32(200, 200, 200, 255);
    case TrackType::POSITION_X: return IM_COL32(255, 100, 100, 255);
    case TrackType::POSITION_Y: return IM_COL32(100, 255, 100, 255);
    case TrackType::POSITION_Z: return IM_COL32(100, 100, 255, 255);
    case TrackType::ROTATION_X: return IM_COL32(255, 200, 100, 255);
    case TrackType::ROTATION_Y: return IM_COL32(200, 255, 100, 255);
    case TrackType::ROTATION_Z: return IM_COL32(100, 200, 255, 255);
    case TrackType::FOV: return IM_COL32(255, 255, 100, 255);
    default: return IM_COL32(150, 150, 150, 255);
    }
  }

  void CameraAnimationTimeline::SetZoom(float zoom) {
    zoom_ = std::max<float>(minZoom_, std::min<float>(maxZoom_, zoom));
    ClampOffset();
  }

  void CameraAnimationTimeline::SetOffset(float offset) {
    offset_ = offset;
    ClampOffset();
  }

  void CameraAnimationTimeline::ClampOffset() {
    if (!animation_) return;

    // キャンバス幅を 800px と仮定
    float visibleWidth = 800.0f - trackLabelWidth_;
    float visibleTime = visibleWidth / (100.0f * zoom_);
    float duration = animation_->GetDuration();

    // 開始より前は見せない
    offset_ = std::max<float>(-1.0f, offset_);

    if (visibleTime >= duration + 2.0f) {
      // 全体が収まる場合は左寄せ
      offset_ = 0.0f;
    }
    else {
      // 終端より先を見せすぎない
      offset_ = std::min<float>(offset_, duration - visibleTime + 1.0f);
    }
  }

} // namespace Tako

#endif // _DEBUG
