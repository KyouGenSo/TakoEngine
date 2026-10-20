#ifdef _DEBUG

#include "CameraAnimationEditor.h"
#include "DebugUIManager.h"
#include "DebugViewport.h"
#include "FrameTimer.h"
#include "LineRenderer.h"
#include "Renderer.h"
#include "Vec3Func.h"
#include "ImGuiManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <functional>
#include <numbers>

namespace Tako {

  namespace {
    using LogType = DebugUIManager::LogType;

    constexpr const char* kNamePopupId = "Animation Name##CameraAnimationEditor";
  } // namespace

  void CameraAnimationEditor::Update() {
    // シーンの切り替えなどでインスタンスごと消えた編集は戻せないので知らせる
    const std::vector<CameraAnimation*>& instances = CameraAnimation::GetInstances();
    const bool isLost = animation_ && animation_ != ownedAnimation_.get() && std::ranges::find(instances, animation_) == instances.end();
    if (isLost) {
      if (tracker_.IsEdited()) {
        LogToConsole(std::format("Camera Animation Editor: unsaved changes to '{}' were lost because the animation was unloaded", name_), LogType::Warning);
      }
      animation_ = nullptr;
      tracker_.Reset();
    }

    // 開いている名前をシーンが読み込んでいれば、そのインスタンスを編集する（ゲームにすぐ反映される）。
    // 消えた場合は新しいシーンのインスタンスか、無ければファイルから開き直す
    if (!name_.empty()) {
      const bool isOwned = !animation_ || animation_ == ownedAnimation_.get();
      if (isLost || (isOwned && !tracker_.IsEdited() && FindSceneAnimation(name_))) {
        if (!Open(name_)) {
          name_.clear();
        }
      }
    }

    if (!animation_) {
      isPlaying_ = false;
    }
    else {
      timeline_.Update(FrameTimer::GetInstance()->GetUnscaledDeltaTime());

      // F7 でゲームを止めていても再生できるよう、止まらない deltaTime で進める
      const float duration = animation_->GetDuration();
      if (isPlaying_) {
        playhead_ += FrameTimer::GetInstance()->GetUnscaledDeltaTime() * animation_->GetPlaySpeed();
        if (duration <= 0.0f) {
          isPlaying_ = false;
        }
        else if (playhead_ > duration || playhead_ < 0.0f) {
          if (animation_->IsLooping()) {
            playhead_ = std::fmod(playhead_, duration);
            if (playhead_ < 0.0f) {
              playhead_ += duration;
            }
          }
          else {
            isPlaying_ = false;
          }
        }
      }
      playhead_ = std::clamp(playhead_, 0.0f, duration);
    }

    // シーン更新の後に書くので、ゲームのコントローラーが書いた値より後勝ちになる
    Camera* camera = MainCamera();
    const bool isPreviewing = *isOpen_ && isPreviewEnabled_ && animation_ && animation_->GetKeyframeCount() > 0 && camera;
    if (isPreviewing) {
      if (!cameraBeforePreview_) {
        cameraBeforePreview_ = CameraAnimation::Pose{ .position = camera->GetTranslate(), .rotation = camera->GetRotate(), .fov = camera->GetFovY() };
      }
      lastPreviewPose_ = animation_->EvaluatePoseAtTime(playhead_);
      camera->SetTranslate(lastPreviewPose_.position);
      camera->SetRotate(lastPreviewPose_.rotation);
      camera->SetFovY(lastPreviewPose_.fov);
    }
    else if (cameraBeforePreview_) {
      // ゲームが書き直していない値だけプレビュー前へ戻す。毎フレーム書かれる位置まで戻すと、1 フレームだけ古い位置が映る
      if (camera) {
        if (camera->GetTranslate() == lastPreviewPose_.position) {
          camera->SetTranslate(cameraBeforePreview_->position);
        }
        if (camera->GetRotate() == lastPreviewPose_.rotation) {
          camera->SetRotate(cameraBeforePreview_->rotation);
        }
        if (camera->GetFovY() == lastPreviewPose_.fov) {
          camera->SetFovY(cameraBeforePreview_->fov);
        }
      }
      cameraBeforePreview_.reset();
    }
  }

  void CameraAnimationEditor::Draw() {
    if (!*isOpen_) {
      return;
    }

    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Camera Animation Editor", isOpen_, ImGuiWindowFlags_MenuBar)) {
      ImGui::End();
      return;
    }

    // UI を描く区間の前後で保存内容を比べて編集を検出する
    tracker_.BeginSection();

    // 区間の最初で受ける（Ctrl+S はこのフレームの編集の前に保存し、Delete などの編集は区間の中で比べる）
    if (animation_) {
      ProcessShortcuts(Shortcuts(), *this);
    }

    DrawMenuBar();
    DrawToolbar();

    if (!animation_) {
      ImGui::Separator();
      ImGui::TextDisabled("Select an animation from the list, or create one with New...");
      tracker_.EndSection();
      ImGui::End();
      return;
    }

    DrawPathVisualization();

    DrawPlaybackControls();

    ImGui::Separator();

    float availHeight = ImGui::GetContentRegionAvail().y - 25.0f; // ステータスバー分を除く
    float topHeight = availHeight * 0.4f;
    float bottomHeight = availHeight * 0.6f;

    // 上段：タイムライン
    if (ImGui::BeginChild("TimelineSection", ImVec2(0, topHeight), true)) {
      DrawTimelinePanel();
    }
    ImGui::EndChild();

    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = availWidth * 0.7f;
    float rightWidth = availWidth * 0.3f;

    // 下段左：カーブエディター
    if (ImGui::BeginChild("CurveSection", ImVec2(leftWidth - 5, bottomHeight), true)) {
      curveEditor_.Draw();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // 下段右：インスペクター
    if (ImGui::BeginChild("InspectorSection", ImVec2(rightWidth - 5, bottomHeight), true)) {
      DrawInspectorPanel();
    }
    ImGui::EndChild();

    DrawStatusBar();

    tracker_.EndSection();

    ImGui::End();
  }

  void CameraAnimationEditor::CollectUnsavedChanges(std::vector<std::string>& out) const {
    if (animation_ && tracker_.IsEdited()) {
      out.push_back("Camera Animation Editor: " + name_);
    }
  }

  std::span<const ShortcutBinding<CameraAnimationEditor>> CameraAnimationEditor::Shortcuts() {
    // 並び順が Help → Shortcuts の一覧の順。Undo / Redo は EditTracker が受ける
    static constexpr ShortcutBinding<CameraAnimationEditor> kShortcuts[] = {
      { ImGuiKey_Space,             ImGuiInputFlags_None,   "Play / Pause the preview", [](CameraAnimationEditor& editor) { editor.TogglePlay(); } },
      { ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_None,   "Copy",                     [](CameraAnimationEditor& editor) { editor.CopySelectedKeyframes(); } },
      { ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_Repeat, "Paste at the playhead",    [](CameraAnimationEditor& editor) { editor.PasteKeyframes(); } },
      { ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_None,   "Select All",               [](CameraAnimationEditor& editor) { editor.timeline_.SelectAll(); } },
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None,   "Save",                     [](CameraAnimationEditor& editor) { editor.Save(); } },
      { ImGuiKey_Delete,            ImGuiInputFlags_None,   "Delete Selected",          [](CameraAnimationEditor& editor) { editor.DeleteSelectedKeyframes(); } },
      { ImGuiKey_Escape,            ImGuiInputFlags_None,   "Deselect",                 [](CameraAnimationEditor& editor) { editor.timeline_.ClearSelection(); } },
    };
    return kShortcuts;
  }

  void CameraAnimationEditor::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
      return;
    }

    bool isShortcutsRequested = false;

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New...")) {
        OpenNamePopup(NamePopup::New);
      }

      if (ImGui::MenuItem("Save", "Ctrl+S", false, animation_ != nullptr)) {
        Save();
      }

      if (ImGui::MenuItem("Save Copy As...", nullptr, false, animation_ != nullptr)) {
        OpenNamePopup(NamePopup::SaveCopy);
      }

      if (ImGui::MenuItem("Reload", nullptr, false, animation_ != nullptr)) {
        RequestReload();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Close")) {
        *isOpen_ = false;
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit", animation_ != nullptr)) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, tracker_.CanUndo())) {
        tracker_.Undo();
      }

      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, tracker_.CanRedo())) {
        tracker_.Redo();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Copy", "Ctrl+C", false, !selectedKeyframes_.empty())) {
        CopySelectedKeyframes();
      }

      if (ImGui::MenuItem("Paste", "Ctrl+V", false, !clipboard_.empty())) {
        PasteKeyframes();
      }

      if (ImGui::MenuItem("Delete", "Del", false, !selectedKeyframes_.empty())) {
        DeleteSelectedKeyframes();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Select All", "Ctrl+A")) {
        timeline_.SelectAll();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Animation", animation_ != nullptr)) {
      if (ImGui::MenuItem("Add Keyframe from Camera", nullptr, false, CaptureSourceCamera() != nullptr)) {
        animation_->AddKeyframe(CaptureCamera(playhead_, CameraKeyframe::CoordinateType::WORLD));
      }

      ImGui::MenuItem("Show Path", nullptr, &showPath_);

      ImGui::Separator();

      if (ImGui::MenuItem("Delete Selected", "Delete", false, !selectedKeyframes_.empty())) {
        DeleteSelectedKeyframes();
      }

      if (ImGui::MenuItem("Clear All Keyframes", nullptr, false, animation_->GetKeyframeCount() > 0)) {
        animation_->ClearKeyframes();
        timeline_.ClearSelection();
      }

      ImGui::Separator();

      ImGui::MenuItem("Enable Grid Snap", nullptr, &enableGridSnap_);

      ImGui::Separator();

      if (ImGui::BeginMenu("Grid Snap Interval")) {
        if (ImGui::MenuItem("0.1 sec", nullptr, gridSnapInterval_ == 0.1f)) {
          gridSnapInterval_ = 0.1f;
        }
        if (ImGui::MenuItem("0.25 sec", nullptr, gridSnapInterval_ == 0.25f)) {
          gridSnapInterval_ = 0.25f;
        }
        if (ImGui::MenuItem("0.5 sec", nullptr, gridSnapInterval_ == 0.5f)) {
          gridSnapInterval_ = 0.5f;
        }
        if (ImGui::MenuItem("1.0 sec", nullptr, gridSnapInterval_ == 1.0f)) {
          gridSnapInterval_ = 1.0f;
        }
        ImGui::EndMenu();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("Shortcuts")) {
        isShortcutsRequested = true;
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();

    // メニューの中で OpenPopup するとメニュー側のウィンドウの ID になり、下の BeginPopupModal と一致しない
    if (isShortcutsRequested) {
      ImGui::OpenPopup("ShortcutsPopup");
    }

    if (ImGui::BeginPopupModal("ShortcutsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Keyboard Shortcuts:");
      ImGui::Separator();
      DrawShortcutTable(Shortcuts());
      // 同じウィンドウで 2 つ目の表を描くので ID を分ける
      ImGui::PushID("History");
      DrawShortcutTable(HistoryShortcuts());
      ImGui::PopID();
      ImGui::Separator();
      if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }

  void CameraAnimationEditor::DrawToolbar() {
    std::string comboLabel = name_.empty() ? "(none)" : name_;
    if (animation_ && animation_ == ownedAnimation_.get()) {
      comboLabel += " (not in scene)";
    }
    if (tracker_.IsEdited()) {
      comboLabel += " *";
    }
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("Animation", comboLabel.c_str())) {
      std::vector<std::string> sceneNames;
      for (CameraAnimation* candidate : CameraAnimation::GetInstances()) {
        const std::string& name = candidate->GetAnimationName();
        if (candidate == ownedAnimation_.get() || std::ranges::find(sceneNames, name) != sceneNames.end()) {
          continue;
        }
        sceneNames.push_back(name);
        if (ImGui::Selectable(name.c_str(), name == name_)) {
          RequestOpen(name);
        }
      }

      // 今のシーンが読み込んでいないファイルは、エディタ自前のインスタンスで開く
      ImGui::SeparatorText("Not in scene");
      for (const std::string& name : ListAnimationFiles()) {
        if (std::ranges::find(sceneNames, name) != sceneNames.end()) {
          continue;
        }
        if (ImGui::Selectable(name.c_str(), name == name_)) {
          RequestOpen(name);
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Animations loaded by the scene are edited in place, so the game uses the edits right away.");

    ImGui::SameLine();
    if (ImGui::Button("New...")) {
      OpenNamePopup(NamePopup::New);
    }

    ImGui::BeginDisabled(!animation_);
    ImGui::SameLine();
    if (ImGui::Button(tracker_.IsEdited() ? "Save *###Save" : "Save###Save")) {
      Save();
    }
    ImGui::SetItemTooltip("Ctrl+S");
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      RequestReload();
    }
    ImGui::SetItemTooltip("Discard unsaved changes and read the file again.");
    ImGui::SameLine();
    tracker_.DrawButtons();
    ImGui::EndDisabled();

    DrawNamePopup();

    std::function<bool()> save;
    if (animation_) {
      save = [this] { return Save(); };
    }
    unsavedPrompt_.Draw("Unsaved Changes##CameraAnimationEditor", save);
  }

  void CameraAnimationEditor::DrawNamePopup() {
    if (isNamePopupRequested_) {
      ImGui::OpenPopup(kNamePopupId);
      isNamePopupRequested_ = false;
    }
    if (!ImGui::BeginPopup(kNamePopupId)) {
      return;
    }

    const bool isNew = namePopup_ == NamePopup::New;
    ImGui::TextUnformatted(isNew ? "New animation name:" : "Save a copy as:");
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool isEntered = ImGui::InputText("##Name", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);

    const std::string name = nameBuffer_;
    const bool        isTaken = IsNameTaken(name);
    if (isTaken) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "'%s' already exists", name.c_str());
    }

    const bool canApply = !name.empty() && !isTaken;
    ImGui::BeginDisabled(!canApply);
    if (ImGui::Button(isNew ? "Create" : "Save") || (isEntered && canApply)) {
      if (isNew) {
        unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("'{}' has unsaved changes.", name_), "Create", [this, name] {
          RevertSceneAnimation();
          CreateNew(name);
        });
      }
      else {
        SaveCopy(name);
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  void CameraAnimationEditor::DrawPathVisualization() {
    if (!showPath_ || animation_->GetKeyframeCount() < 2) {
      return;
    }

    auto* lineRenderer = LineRenderer::GetInstance();
    const Vector4 pathColor = { 0.2f, 0.9f, 0.9f, 1.0f };
    const Vector4 keyframeColor = { 1.0f, 0.8f, 0.2f, 1.0f };
    const Vector4 selectedColor = { 1.0f, 0.2f, 0.2f, 1.0f };

    // 実際の再生と同じ関数で位置を計算して線を引く（表示と再生のズレを防ぐ）
    for (size_t i = 0; i + 1 < animation_->GetKeyframeCount(); ++i) {
      float t1 = animation_->GetKeyframe(i).time;
      float t2 = animation_->GetKeyframe(i + 1).time;

      Vector3 prevPos = animation_->EvaluatePoseAtTime(t1).position;
      for (int step = 1; step <= kPathDivision; ++step) {
        float time = t1 + (t2 - t1) * static_cast<float>(step) / kPathDivision;
        Vector3 pos = animation_->EvaluatePoseAtTime(time).position;
        lineRenderer->DrawLine(prevPos, pos, pathColor);
        prevPos = pos;
      }
    }

    for (size_t i = 0; i < animation_->GetKeyframeCount(); ++i) {
      bool isSelected = timeline_.IsSelected(animation_->GetKeyframe(i).id);
      Vector3 pos = animation_->EvaluatePoseAtTime(animation_->GetKeyframe(i).time).position;
      lineRenderer->DrawSphere(pos, 0.15f, isSelected ? selectedColor : keyframeColor, 8);
    }
  }

  void CameraAnimationEditor::DrawPlaybackControls() {
    const float duration = animation_->GetDuration();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15, 8));

    if (ImGui::Button(isPlaying_ ? "||" : ">", ImVec2(40, 0))) {
      TogglePlay();
    }

    ImGui::SameLine();
    if (ImGui::Button("[]", ImVec2(40, 0))) {
      isPlaying_ = false;
      playhead_ = 0.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button("|<", ImVec2(40, 0))) {
      playhead_ = 0.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(40, 0))) {
      playhead_ = duration;
    }

    ImGui::SameLine();
    ImGui::Text("Time: %.2f / %.2f", playhead_, duration);

    ImGui::SameLine();
    bool isLooping = animation_->IsLooping();
    if (ImGui::Checkbox("Loop", &isLooping)) {
      animation_->SetLooping(isLooping);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    float playSpeed = animation_->GetPlaySpeed();
    if (ImGui::DragFloat("Speed", &playSpeed, 0.01f, -2.0f, 2.0f, "%.2fx")) {
      animation_->SetPlaySpeed(playSpeed);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Preview", &isPreviewEnabled_);
    ImGui::SetItemTooltip("While on, the main camera follows the playhead (overrides the game camera). The start blend of Smooth Blend is not previewed.");

    ImGui::PopStyleVar();

    ImGui::SetNextItemWidth(140);
    int timingMode = static_cast<int>(animation_->GetTimingMode());
    if (ImGui::Combo("Timing", &timingMode, "Per Segment\0Unified Warp\0Constant Speed\0")) {
      animation_->SetTimingMode(static_cast<CameraAnimation::TimingMode>(timingMode));
    }

    if (animation_->GetTimingMode() != CameraAnimation::TimingMode::PER_SEGMENT) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120);
      int globalEasing = static_cast<int>(animation_->GetGlobalEasing());
      if (ImGui::Combo("Global Easing", &globalEasing, "Linear\0Ease In\0Ease Out\0Ease In-Out\0Cubic Bezier\0")) {
        animation_->SetGlobalEasing(static_cast<CameraKeyframe::InterpolationType>(globalEasing));
      }

      if (animation_->GetGlobalEasing() == CameraKeyframe::InterpolationType::CUBIC_BEZIER) {
        Vector2 p1 = animation_->GetGlobalBezierP1();
        Vector2 p2 = animation_->GetGlobalBezierP2();
        ImGui::SetNextItemWidth(120);
        if (ImGui::DragFloat2("Global P1", &p1.x, 0.01f)) {
          animation_->SetGlobalBezierP1(p1);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::DragFloat2("Global P2", &p2.x, 0.01f)) {
          animation_->SetGlobalBezierP2(p2);
        }
      }
    }

    // スクラブ用スライダー。プレビュー中は背景色を変える
    if (duration > 0.0f) {
      if (isPreviewEnabled_) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.2f, 0.4f, 1.0f));
      }
      ImGui::SliderFloat("##Timeline", &playhead_, 0.0f, duration, "%.2fs");
      if (isPreviewEnabled_) {
        ImGui::PopStyleColor();
      }
    }
  }

  void CameraAnimationEditor::DrawTimelinePanel() {
    timeline_.SetGridSnapEnabled(enableGridSnap_);
    timeline_.SetGridSnapInterval(gridSnapInterval_);
    timeline_.Draw(playhead_);

    // タイムライン側の状態を取り込む
    selectedKeyframes_ = timeline_.GetSelectedKeyframes();
  }

  void CameraAnimationEditor::DrawInspectorPanel() {
    ImGui::Text("Inspector");
    ImGui::Separator();

    const char* captureSources[] = { "Main Camera", "Debug Viewport" };
    int captureSourceIndex = static_cast<int>(captureSource_);
    if (ImGui::Combo("Capture From", &captureSourceIndex, captureSources, 2)) {
      captureSource_ = static_cast<CaptureSource>(captureSourceIndex);
    }
    if (captureSource_ == CaptureSource::DebugViewport && !CaptureSourceCamera()) {
      ImGui::TextDisabled("Open the Debug Viewport (F1)");
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Start Mode Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      const char* startModes[] = { "Jump Cut", "Smooth Blend" };
      int startModeIndex = static_cast<int>(animation_->GetStartMode());
      if (ImGui::Combo("Start Mode", &startModeIndex, startModes, 2)) {
        animation_->SetStartMode(static_cast<CameraAnimation::StartMode>(startModeIndex));
      }

      if (startModeIndex == 0) {
        ImGui::TextWrapped("Jump Cut: Instantly moves to the first keyframe when playback starts.");
      }
      else {
        ImGui::TextWrapped("Smooth Blend: Smoothly transitions from current camera position to the first keyframe.");
      }

      if (animation_->GetStartMode() == CameraAnimation::StartMode::SMOOTH_BLEND) {
        float blendDuration = animation_->GetBlendDuration();
        if (ImGui::DragFloat("Blend Duration (sec)", &blendDuration, 0.01f, 0.1f, 2.0f)) {
          animation_->SetBlendDuration(blendDuration);
        }

        if (animation_->IsBlending()) {
          ImGui::ProgressBar(animation_->GetBlendProgress(), ImVec2(-1, 0), "Blending...");
        }
      }

      ImGui::Separator();
    }

    const Transform* target = animation_->GetTarget();
    if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("Current Target: ");
      ImGui::SameLine();
      if (target) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Set by the game");
        ImGui::TextWrapped("Keyframes with TARGET_RELATIVE coordinate type use this target as reference.");
      }
      else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "None");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
          "TARGET_RELATIVE keyframes will use world coordinates. The game sets the target with CameraAnimation::SetTarget().");
      }

      ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Add New Keyframe")) {
      static float newKeyTime = 0.0f;
      static int coordTypeIndex = 0; // 0: WORLD, 1: TARGET_RELATIVE

      ImGui::DragFloat("Time (seconds)", &newKeyTime, 0.01f, 0.0f, 10.0f);

      const char* coordTypes[] = { "World", "Target Relative" };
      ImGui::Combo("Coordinate Type##AddFrame", &coordTypeIndex, coordTypes, 2);

      if (coordTypeIndex == 1 && !target) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
          "Warning: No target set. Will use world coordinates.");
      }

      const float time = enableGridSnap_ ? SnapToGrid(newKeyTime) : newKeyTime;
      const auto coordinateType = static_cast<CameraKeyframe::CoordinateType>(coordTypeIndex);

      ImGui::BeginDisabled(CaptureSourceCamera() == nullptr);
      if (ImGui::Button("Add from Current Camera")) {
        animation_->AddKeyframe(CaptureCamera(time, coordinateType));

        // 次の追加に備えて時刻を進める
        newKeyTime = newKeyTime + 1.0f;
      }
      ImGui::EndDisabled();

      ImGui::SameLine();
      if (ImGui::Button("Add Default")) {
        CameraKeyframe defaultKf;
        defaultKf.time = time;
        defaultKf.position = Vector3(0.0f, 5.0f, -10.0f);
        defaultKf.rotation = Vector3(0.2f, 0.0f, 0.0f);
        defaultKf.fov = 45.0f * std::numbers::pi_v<float> / 180.0f;
        defaultKf.interpolation = CameraKeyframe::InterpolationType::LINEAR;
        defaultKf.coordinateType = coordinateType;

        animation_->AddKeyframe(defaultKf);

        newKeyTime = newKeyTime + 1.0f;
      }

      ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Keyframe Param")) {

      if (selectedKeyframes_.empty()) {
        ImGui::TextDisabled("No keyframe selected");
        return;
      }

      if (selectedKeyframes_.size() == 1) {
        const int idx = animation_->FindKeyframeIndex(selectedKeyframes_[0]);
        if (idx >= 0) {
          CameraKeyframe kf = animation_->GetKeyframe(idx);
          bool changed = false;

          ImGui::Text("Keyframe %d", idx);
          ImGui::Separator();

          if (ImGui::DragFloat("Time", &kf.time, 0.01f, 0.0f, animation_->GetDuration())) {
            if (enableGridSnap_) {
              kf.time = SnapToGrid(kf.time);
            }
            changed = true;
          }

          const char* coordTypes[] = { "World", "Target Relative" };
          int currentCoordType = static_cast<int>(kf.coordinateType);
          if (ImGui::Combo("Coordinate Type##FrameParam", &currentCoordType, coordTypes, 2)) {
            kf.coordinateType = static_cast<CameraKeyframe::CoordinateType>(currentCoordType);
            changed = true;
          }

          if (kf.coordinateType == CameraKeyframe::CoordinateType::TARGET_RELATIVE) {
            if (target) {
              ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.8f, 1.0f), "Position is offset from target");
            }
            else {
              ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No target! Using world coordinates");
            }
          }

          const char* posLabel = (kf.coordinateType == CameraKeyframe::CoordinateType::TARGET_RELATIVE)
            ? "Position (Offset)" : "Position";
          if (ImGui::DragFloat3(posLabel, &kf.position.x, 0.1f)) {
            changed = true;
          }

          // 度数で表示・編集（内部はラジアン）
          Vector3 rotDeg = {
            kf.rotation.x * 180.0f / std::numbers::pi_v<float>,
            kf.rotation.y * 180.0f / std::numbers::pi_v<float>,
            kf.rotation.z * 180.0f / std::numbers::pi_v<float>
          };
          if (ImGui::DragFloat3("Rotation", &rotDeg.x, 1.0f)) {
            kf.rotation = {
              rotDeg.x * std::numbers::pi_v<float> / 180.0f,
              rotDeg.y * std::numbers::pi_v<float> / 180.0f,
              rotDeg.z * std::numbers::pi_v<float> / 180.0f
            };
            changed = true;
          }

          // 度数で表示・編集（内部はラジアン）
          float fovDeg = kf.fov * 180.0f / std::numbers::pi_v<float>;
          if (ImGui::DragFloat("FOV", &fovDeg, 0.5f, 10.0f, 120.0f)) {
            kf.fov = fovDeg * std::numbers::pi_v<float> / 180.0f;
            changed = true;
          }

          const char* interpTypes[] = { "Linear", "Ease In", "Ease Out", "Ease In-Out", "Cubic Bezier" };
          int currentType = static_cast<int>(kf.interpolation);
          if (ImGui::Combo("Interpolation", &currentType, interpTypes, 5)) {
            kf.interpolation = static_cast<CameraKeyframe::InterpolationType>(currentType);
            changed = true;
          }

          if (animation_->GetTimingMode() != CameraAnimation::TimingMode::PER_SEGMENT) {
            ImGui::TextDisabled("Interpolation is ignored in current timing mode");
          }

          if (kf.interpolation == CameraKeyframe::InterpolationType::CUBIC_BEZIER) {
            if (ImGui::DragFloat2("Bezier P1", &kf.bezierP1.x, 0.01f)) {
              kf.bezierP1.x = std::clamp(kf.bezierP1.x, 0.0f, 1.0f);
              changed = true;
            }
            if (ImGui::DragFloat2("Bezier P2", &kf.bezierP2.x, 0.01f)) {
              kf.bezierP2.x = std::clamp(kf.bezierP2.x, 0.0f, 1.0f);
              changed = true;
            }
          }

          const char* pathTypes[] = { "Linear", "Catmull-Rom" };
          int currentPathType = static_cast<int>(kf.pathType);
          if (ImGui::Combo("Path Type", &currentPathType, pathTypes, 2)) {
            kf.pathType = static_cast<CameraKeyframe::PathType>(currentPathType);
            changed = true;
          }

          if (ImGui::Button("Apply Path to All Keyframes")) {
            for (size_t i = 0; i < animation_->GetKeyframeCount(); ++i) {
              CameraKeyframe pathKf = animation_->GetKeyframe(i);
              pathKf.pathType = kf.pathType;
              animation_->EditKeyframe(i, pathKf);
            }
          }

          if (changed) {
            animation_->EditKeyframe(idx, kf);
          }

          ImGui::BeginDisabled(CaptureSourceCamera() == nullptr);
          if (ImGui::Button("Apply Current Camera")) {
            const CameraKeyframe captured = CaptureCamera(kf.time, kf.coordinateType);
            kf.position = captured.position;
            kf.rotation = captured.rotation;
            kf.fov = captured.fov;
            animation_->EditKeyframe(animation_->FindKeyframeIndex(kf.id), kf);
          }
          ImGui::EndDisabled();

          ImGui::Spacing();
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f));
          if (ImGui::Button("Delete Keyframe", ImVec2(-1, 0))) {
            DeleteSelectedKeyframes();
          }
          ImGui::PopStyleColor(3);
        }
      }
      else {
        ImGui::Text("%d keyframes selected", static_cast<int>(selectedKeyframes_.size()));
        ImGui::Separator();

        // 適用はボタン押下時にまとめて実行
        static Vector3 offsetPos = { 0, 0, 0 };
        ImGui::DragFloat3("Offset Position", &offsetPos.x, 0.1f);

        if (ImGui::Button("Apply Offset")) {
          for (uint32_t id : selectedKeyframes_) {
            const int idx = animation_->FindKeyframeIndex(id);
            if (idx >= 0) {
              CameraKeyframe kf = animation_->GetKeyframe(idx);
              kf.position.x += offsetPos.x;
              kf.position.y += offsetPos.y;
              kf.position.z += offsetPos.z;
              animation_->EditKeyframe(idx, kf);
            }
          }
          offsetPos = { 0, 0, 0 };
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Delete Selected Keyframes", ImVec2(-1, 0))) {
          DeleteSelectedKeyframes();
        }
        ImGui::PopStyleColor(3);
      }
    }
  }

  void CameraAnimationEditor::DrawStatusBar() {
    ImGui::Separator();

    if (isPreviewEnabled_) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.8f, 1.0f));
      ImGui::Text("[PREVIEW]");
      ImGui::PopStyleColor();
      ImGui::SameLine();
    }

    ImGui::Text("Keyframes: %zu | Selected: %zu | Snap: %s (%.2fs)",
      animation_->GetKeyframeCount(),
      selectedKeyframes_.size(),
      enableGridSnap_ ? "ON" : "OFF",
      gridSnapInterval_);
  }

  bool CameraAnimationEditor::Open(const std::string& name) {
    CameraAnimation*                 animation = FindSceneAnimation(name);
    std::unique_ptr<CameraAnimation> owned;
    if (!animation) {
      owned = std::make_unique<CameraAnimation>();
      if (!owned->LoadFromJson(name)) {
        return false;
      }
      animation = owned.get();
    }

    ownedAnimation_ = std::move(owned);
    animation_      = animation;
    name_           = name;

    timeline_.Initialize(animation_);
    curveEditor_.Initialize(animation_);
    selectedKeyframes_.clear();
    playhead_  = 0.0f;
    isPlaying_ = false;
    tracker_.Reset();
    return true;
  }

  void CameraAnimationEditor::RequestOpen(const std::string& name) {
    if (name == name_ && animation_) {
      return;
    }
    unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("'{}' has unsaved changes.", name_), "Switch", [this, name] {
      RevertSceneAnimation();
      if (!Open(name)) {
        LogToConsole(std::format("Camera Animation Editor: failed to open '{}'", name), LogType::Error);
      }
    });
  }

  void CameraAnimationEditor::RevertSceneAnimation() {
    if (tracker_.IsEdited() && animation_ && animation_ != ownedAnimation_.get()) {
      animation_->LoadFromJson(name_);
    }
  }

  void CameraAnimationEditor::RequestReload() {
    unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("'{}' has unsaved changes.", name_), "Reload", [this] {
      if (animation_ && animation_->LoadFromJson(name_)) {
        tracker_.Reset();
      }
      else {
        LogToConsole(std::format("Camera Animation Editor: failed to reload '{}'", name_), LogType::Error);
      }
    });
  }

  void CameraAnimationEditor::OpenNamePopup(NamePopup purpose) {
    namePopup_            = purpose;
    nameBuffer_[0]        = '\0';
    isNamePopupRequested_ = true;
  }

  bool CameraAnimationEditor::Save() {
    if (!animation_ || !animation_->SaveToJson(name_)) {
      LogToConsole(std::format("Camera Animation Editor: failed to save '{}'", name_), LogType::Error);
      return false;
    }
    tracker_.MarkSaved();
    return true;
  }

  bool CameraAnimationEditor::SaveCopy(const std::string& name) {
    if (!animation_ || !animation_->SaveToJson(name)) {
      LogToConsole(std::format("Camera Animation Editor: failed to save a copy as '{}'", name), LogType::Error);
      return false;
    }
    return true;
  }

  bool CameraAnimationEditor::CreateNew(const std::string& name) {
    CameraAnimation animation;
    if (!animation.SaveToJson(name) || !Open(name)) {
      LogToConsole(std::format("Camera Animation Editor: failed to create '{}'", name), LogType::Error);
      return false;
    }
    return true;
  }

  bool CameraAnimationEditor::IsNameTaken(const std::string& name) const {
    if (name.empty()) {
      return false;
    }
    return std::filesystem::exists(std::filesystem::path(CameraAnimation::kDirectory) / (name + ".json")) || FindSceneAnimation(name);
  }

  CameraAnimation* CameraAnimationEditor::FindSceneAnimation(const std::string& name) const {
    for (CameraAnimation* animation : CameraAnimation::GetInstances()) {
      // ponytail: 同じ名前のインスタンスが複数あれば最初の 1 つだけ編集する。複数のコントローラーが同じファイルを読むようになったら全部へ反映する
      if (animation != ownedAnimation_.get() && animation->GetAnimationName() == name) {
        return animation;
      }
    }
    return nullptr;
  }

  std::vector<std::string> CameraAnimationEditor::ListAnimationFiles() {
    std::vector<std::string> names;
    std::error_code          ec;
    for (const auto& entry : std::filesystem::directory_iterator(CameraAnimation::kDirectory, ec)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        names.push_back(entry.path().stem().string());
      }
    }
    std::ranges::sort(names);
    return names;
  }

  Camera* CameraAnimationEditor::MainCamera() {
    return *Renderer::GetInstance()->GetCamera();
  }

  float CameraAnimationEditor::SnapToGrid(float time) const {
    if (!enableGridSnap_) {
      return time;
    }
    return std::round(time / gridSnapInterval_) * gridSnapInterval_;
  }

  const Camera* CameraAnimationEditor::CaptureSourceCamera() const {
    return captureSource_ == CaptureSource::DebugViewport ? debugViewport_->GetCamera() : MainCamera();
  }

  CameraKeyframe CameraAnimationEditor::CaptureCamera(float time, CameraKeyframe::CoordinateType coordinateType) const {
    const Camera* camera = CaptureSourceCamera();

    CameraKeyframe keyframe;
    keyframe.time           = time;
    keyframe.coordinateType = coordinateType;
    keyframe.position       = camera->GetTranslate();
    keyframe.rotation       = camera->GetRotate();
    keyframe.fov            = camera->GetFovY();

    // TARGET_RELATIVE では現在位置からターゲット位置を引いてオフセット化
    const Transform* target = animation_->GetTarget();
    if (coordinateType == CameraKeyframe::CoordinateType::TARGET_RELATIVE && target) {
      keyframe.position = Vec3::Subtract(keyframe.position, target->translate);
    }
    return keyframe;
  }

  void CameraAnimationEditor::TogglePlay() {
    if (!animation_) {
      return;
    }
    // 末尾で止まっていたら頭から再生する
    if (!isPlaying_ && playhead_ >= animation_->GetDuration()) {
      playhead_ = 0.0f;
    }
    isPlaying_ = !isPlaying_;
  }

  void CameraAnimationEditor::CopySelectedKeyframes() {
    clipboard_.clear();
    for (uint32_t id : selectedKeyframes_) {
      const int idx = animation_->FindKeyframeIndex(id);
      if (idx >= 0) {
        clipboard_.push_back(animation_->GetKeyframe(idx));
      }
    }
  }

  void CameraAnimationEditor::PasteKeyframes() {
    if (clipboard_.empty()) {
      return;
    }

    float minTime = clipboard_[0].time;

    // クリップボード内の最小時刻を再生ヘッドに合わせて相対配置
    for (const auto& kf : clipboard_) {
      CameraKeyframe newKf = kf;
      newKf.time = playhead_ + (kf.time - minTime);
      animation_->AddKeyframe(newKf);
    }
  }

  void CameraAnimationEditor::DeleteSelectedKeyframes() {
    if (selectedKeyframes_.empty()) {
      return;
    }

    // 消すたびに後ろの添字がずれるので、毎回 id から引き直す
    for (uint32_t id : selectedKeyframes_) {
      const int idx = animation_->FindKeyframeIndex(id);
      if (idx >= 0) {
        animation_->RemoveKeyframe(idx);
      }
    }

    timeline_.ClearSelection();
    selectedKeyframes_.clear();
  }

} // namespace Tako

#endif // _DEBUG
