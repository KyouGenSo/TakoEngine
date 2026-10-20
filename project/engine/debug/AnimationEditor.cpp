#ifdef _DEBUG

#include "AnimationEditor.h"
#include "DebugUIManager.h"
#include "FrameTimer.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dBasic.h"
#include "PrimitiveBuilder.h"
#include "SpriteBasic.h"
#include "UILayout.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <utility>

namespace Tako {

  namespace {
    using LogType = DebugUIManager::LogType;

    constexpr const char* kNamePopupId     = "Clip Name##AnimationEditor";
    constexpr const char* kAddTrackPopupId = "Add Track##AnimationEditor";
    constexpr const char* kLoopModeLabels[] = { "Once", "Loop", "Ping Pong" };

    /// <summary>
    /// std::string を直接編集する InputText（128 文字まで）
    /// </summary>
    bool InputString(const char* label, std::string& value) {
      char buffer[128];
      strncpy_s(buffer, value.c_str(), _TRUNCATE);
      if (!ImGui::InputText(label, buffer, sizeof(buffer))) {
        return false;
      }
      value = buffer;
      return true;
    }

    void CollectNames(const UIElement& node, std::vector<std::string>& out) {
      for (const std::unique_ptr<UIElement>& child : node.children) {
        out.push_back(child->name);
        CollectNames(*child, out);
      }
    }

    bool EaseCombo(const char* label, EaseType& ease) {
      bool isChanged = false;
      if (ImGui::BeginCombo(label, Ease::ToString(ease), ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < static_cast<int>(EaseType::Count); ++i) {
          const EaseType type = static_cast<EaseType>(i);
          if (ImGui::Selectable(Ease::ToString(type), type == ease)) {
            ease      = type;
            isChanged = true;
          }
        }
        ImGui::EndCombo();
      }
      return isChanged;
    }

    /// <summary>
    /// key から次のキーまでの進み方のグラフ。Back や Elastic は 0～1 をはみ出すので縦は -0.5～1.5 を映す
    /// </summary>
    void DrawEaseGraph(const AnimKey& key) {
      constexpr float kMin    = -0.5f;
      constexpr float kMax    = 1.5f;
      constexpr int   kSample = 64;

      const ImVec2 size = { ImGui::GetContentRegionAvail().x, 90.0f };
      const ImVec2 min  = ImGui::GetCursorScreenPos();
      const ImVec2 max  = { min.x + size.x, min.y + size.y };
      ImGui::Dummy(size);

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      auto toScreen = [&](float t, float value) {
        return ImVec2(min.x + t * size.x, max.y - (value - kMin) / (kMax - kMin) * size.y);
      };
      drawList->AddRectFilled(min, max, IM_COL32(30, 30, 30, 255));
      drawList->AddLine(toScreen(0.0f, 0.0f), toScreen(1.0f, 0.0f), IM_COL32(70, 70, 70, 255));
      drawList->AddLine(toScreen(0.0f, 1.0f), toScreen(1.0f, 1.0f), IM_COL32(70, 70, 70, 255));

      ImVec2 previous = toScreen(0.0f, 0.0f);
      for (int i = 1; i <= kSample; ++i) {
        const float  t       = static_cast<float>(i) / kSample;
        const float  value   = Ease::Evaluate(key.ease, t, key.bezierP1.x, key.bezierP1.y, key.bezierP2.x, key.bezierP2.y);
        const ImVec2 current = toScreen(t, value);
        drawList->AddLine(previous, current, IM_COL32(255, 170, 40, 255), 2.0f);
        previous = current;
      }
    }

    bool IsStepProperty(AnimProperty property) {
      return property == AnimProperty::Frame || property == AnimProperty::Visible;
    }

  } // namespace

  AnimationEditor::AnimationEditor() = default;
  AnimationEditor::~AnimationEditor() = default;

  void AnimationEditor::Initialize(bool* isOpen, const ViewportCameraSettings* cameraSettings) {
    isOpen_ = isOpen;
    preview_.Initialize(L"AnimationPreview", cameraSettings);
  }

  void AnimationEditor::Update() {
    if (!*isOpen_) {
      ReleaseLayoutPreview();
      ownedLayout_.reset();
      previewObject_.reset();
      floorObject_.reset();
      isPlaying_ = false;
      return;
    }

    if (clip_ && isPlaying_) {
      // F7 でゲームを止めていても再生できるよう、止まらない deltaTime で進める
      playElapsed_ += FrameTimer::GetInstance()->GetUnscaledDeltaTime() * previewSpeed_;
      playhead_     = clip_->WrapTime(playElapsed_);
      if (clip_->GetLoopMode() == AnimationClip::LoopMode::Once && playElapsed_ >= clip_->GetDuration()) {
        isPlaying_ = false;
      }
    }
    if (clip_) {
      playhead_ = std::clamp(playhead_, 0.0f, clip_->GetDuration());
    }

    // レイアウトの切り替えはスプライトを作り直すので、描画中ではなくここで行う
    if (!pendingLayoutName_.empty()) {
      layoutName_ = std::exchange(pendingLayoutName_, std::string());
    }
    const bool needsOwnedLayout = previewMode_ == PreviewMode::UI && !layoutName_.empty() && !FindSceneLayout(layoutName_);
    if (needsOwnedLayout && (!ownedLayout_ || ownedLayout_->GetName() != layoutName_)) {
      ReleaseLayoutPreview();
      ownedLayout_ = std::make_unique<UILayout>();
      ownedLayout_->Load(layoutName_);
    }
    else if (!needsOwnedLayout && ownedLayout_) {
      if (previewedLayout_ == ownedLayout_.get()) {
        previewedLayout_ = nullptr;
      }
      ownedLayout_.reset();
    }

    UpdateLayoutPreview();
    UpdateObjectPreview();
  }

  bool AnimationEditor::Draw() {
    if (!*isOpen_) {
      return false;
    }

    ImGui::SetNextWindowSize(ImVec2(1280, 780), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animation Editor", isOpen_, ImGuiWindowFlags_MenuBar)) {
      ImGui::End();
      return false;
    }

    // UI を描く区間の前後で保存内容を比べて編集を検出する
    tracker_.BeginSection();

    // 区間の最初で受ける（Ctrl+S はこのフレームの編集の前に保存し、Delete などの編集は区間の中で比べる）
    if (clip_) {
      ProcessShortcuts(Shortcuts(), *this);
    }

    DrawMenuBar();
    DrawToolbar();

    if (!clip_) {
      ImGui::Separator();
      ImGui::TextDisabled("Select an animation clip from the list, or create one with New...");
      tracker_.EndSection();
      ImGui::End();
      return false;
    }

    DrawPreviewTarget();
    DrawPlaybackControls();
    ImGui::Separator();

    bool         isInputCaptured = false;
    const ImVec2 avail           = ImGui::GetContentRegionAvail();
    const float  bodyHeight      = avail.y - ImGui::GetFrameHeightWithSpacing();

    if (ImGui::BeginChild("Sheet", ImVec2(avail.x * 0.68f, bodyHeight))) {
      if (ImGui::Button("+ Track")) {
        ImGui::OpenPopup(kAddTrackPopupId);
      }
      DrawAddTrackPopup();

      ImGui::SameLine();
      ImGui::SetNextItemWidth(110.0f);
      const float snap = timeline_.GetSnapInterval();
      if (ImGui::BeginCombo("Snap", snap > 0.0f ? std::format("{:.2f} s", snap).c_str() : "Off")) {
        for (float option : kSnapOptions) {
          if (ImGui::Selectable(option > 0.0f ? std::format("{:.2f} s", option).c_str() : "Off", option == snap)) {
            timeline_.SetSnapInterval(option);
          }
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      ImGui::TextDisabled("Double-click a row to add a key / event. Ctrl+Wheel: zoom, Middle-drag: pan, Right-click a label: track menu");

      // 再生中に目盛りで時刻を動かしたら、そこから再生を続ける
      const float playheadBefore = playhead_;
      timeline_.Draw(*clip_, playhead_, ImGui::GetContentRegionAvail().y * 0.62f);
      if (playhead_ != playheadBefore) {
        playElapsed_ = playhead_;
      }
      timeline_.DrawCurve(*clip_, playhead_);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("Side", ImVec2(0.0f, bodyHeight))) {
      if (previewMode_ == PreviewMode::Object3D) {
        preview_.DrawCameraToolbar();
        if (ImGui::BeginChild("PreviewImage", ImVec2(0.0f, kPreviewHeight))) {
          isInputCaptured = preview_.DrawImage();
        }
        ImGui::EndChild();
      }
      DrawInspector();
    }
    ImGui::EndChild();

    DrawStatusBar();

    tracker_.EndSection();
    ImGui::End();
    return isInputCaptured;
  }

  void AnimationEditor::DrawLayoutPreview() {
    // シーンのレイアウトはシーンが描くので、エディタが読み込んだものだけ重ねる
    if (!*isOpen_ || !ownedLayout_ || FindTargetLayout() != ownedLayout_.get()) {
      return;
    }
    SpriteBasic::GetInstance()->SetCommonRenderSetting();
    ownedLayout_->Draw();
  }

  void AnimationEditor::DrawPreviewPass() {
    if (!*isOpen_ || previewMode_ != PreviewMode::Object3D || !preview_.IsReady() || !previewObject_) {
      return;
    }

    preview_.BeginPass();

    // プレビューカメラを視点にし、直前は別パスの PSO のため共通描画設定を再適用
    Object3dBasic::GetInstance()->SetView(preview_.GetCamera());
    Object3dBasic::GetInstance()->SetCommonRenderSetting();

    floorObject_->Draw();
    previewObject_->Draw();

    preview_.EndPass();
  }

  void AnimationEditor::Finalize() {
    previewObject_.reset();
    floorObject_.reset();
    ownedLayout_.reset();
    preview_.Finalize();
  }

  void AnimationEditor::CollectUnsavedChanges(std::vector<std::string>& out) const {
    if (clip_ && tracker_.IsEdited()) {
      out.push_back("Animation Editor: " + name_);
    }
  }

  std::span<const ShortcutBinding<AnimationEditor>> AnimationEditor::Shortcuts() {
    // 並び順が Help → Shortcuts の一覧の順。Undo / Redo は EditTracker が受ける
    static constexpr ShortcutBinding<AnimationEditor> kShortcuts[] = {
      { ImGuiKey_Space,             ImGuiInputFlags_None,   "Play / Pause the preview",                 [](AnimationEditor& editor) { editor.TogglePlay(); } },
      { ImGuiKey_K,                 ImGuiInputFlags_None,   "Key the selected track at the playhead",   [](AnimationEditor& editor) { editor.AddKeyAtPlayhead(); } },
      { ImGuiKey_Home,              ImGuiInputFlags_None,   "Playhead to start",                        [](AnimationEditor& editor) { editor.playhead_ = 0.0f; } },
      { ImGuiKey_End,               ImGuiInputFlags_None,   "Playhead to end",                          [](AnimationEditor& editor) { editor.playhead_ = editor.clip_->GetDuration(); } },
      { ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_None,   "Copy keys",                                [](AnimationEditor& editor) { editor.CopySelection(); } },
      { ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_Repeat, "Paste keys at the playhead",               [](AnimationEditor& editor) { editor.Paste(); } },
      { ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_None,   "Select All",                               [](AnimationEditor& editor) { editor.timeline_.SelectAll(*editor.clip_); } },
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None,   "Save",                                     [](AnimationEditor& editor) { editor.Save(); } },
      { ImGuiKey_Delete,            ImGuiInputFlags_None,   "Delete selected keys and events",          [](AnimationEditor& editor) { editor.DeleteSelection(); } },
      { ImGuiKey_Escape,            ImGuiInputFlags_None,   "Deselect",                                 [](AnimationEditor& editor) { editor.timeline_.ClearSelection(); } },
    };
    return kShortcuts;
  }

  void AnimationEditor::DrawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
      return;
    }

    bool isShortcutsRequested = false;
    const bool hasSelection   = clip_ && !timeline_.GetSelection().empty();

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New...")) {
        OpenNamePopup(NamePopup::New);
      }
      if (ImGui::MenuItem("Save", "Ctrl+S", false, clip_ != nullptr)) {
        Save();
      }
      if (ImGui::MenuItem("Save Copy As...", nullptr, false, clip_ != nullptr)) {
        OpenNamePopup(NamePopup::SaveCopy);
      }
      if (ImGui::MenuItem("Reload", nullptr, false, clip_ != nullptr)) {
        RequestReload();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Close")) {
        *isOpen_ = false;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit", clip_ != nullptr)) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, tracker_.CanUndo())) {
        tracker_.Undo();
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y", false, tracker_.CanRedo())) {
        tracker_.Redo();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) {
        CopySelection();
      }
      if (ImGui::MenuItem("Paste", "Ctrl+V", false, !clipboard_.empty())) {
        Paste();
      }
      if (ImGui::MenuItem("Delete", "Del", false, hasSelection)) {
        DeleteSelection();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Select All", "Ctrl+A")) {
        timeline_.SelectAll(*clip_);
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

  void AnimationEditor::DrawToolbar() {
    const std::string comboLabel = (name_.empty() ? std::string("(none)") : name_) + (tracker_.IsEdited() ? " *" : "");
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("Clip", comboLabel.c_str())) {
      for (const std::string& name : ListJsonFiles(AnimationClip::kDirectory)) {
        if (ImGui::Selectable(name.c_str(), name == name_)) {
          RequestOpen(name);
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Clips are shared by name, so the game plays the edits right away.");

    ImGui::SameLine();
    if (ImGui::Button("New...")) {
      OpenNamePopup(NamePopup::New);
    }

    ImGui::BeginDisabled(!clip_);
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

    if (clip_) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(90.0f);
      float duration = clip_->GetDuration();
      if (ImGui::DragFloat("Duration", &duration, 0.01f, 0.01f, 60.0f, "%.2f s")) {
        clip_->SetDuration(duration);
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100.0f);
      int loopMode = static_cast<int>(clip_->GetLoopMode());
      if (ImGui::Combo("Loop", &loopMode, kLoopModeLabels, static_cast<int>(std::size(kLoopModeLabels)))) {
        clip_->SetLoopMode(static_cast<AnimationClip::LoopMode>(loopMode));
      }
    }

    DrawNamePopup();

    std::function<bool()> save;
    if (clip_) {
      save = [this] { return Save(); };
    }
    unsavedPrompt_.Draw("Unsaved Changes##AnimationEditor", save);
  }

  void AnimationEditor::DrawNamePopup() {
    if (isNamePopupRequested_) {
      ImGui::OpenPopup(kNamePopupId);
      isNamePopupRequested_ = false;
    }
    if (!ImGui::BeginPopup(kNamePopupId)) {
      return;
    }

    const bool isNew = namePopup_ == NamePopup::New;
    ImGui::TextUnformatted(isNew ? "New clip name:" : "Save a copy as:");
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool isEntered = ImGui::InputText("##Name", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);

    const std::string name    = nameBuffer_;
    const bool        isTaken = IsNameTaken(name);
    if (isTaken) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "'%s' already exists", name.c_str());
    }

    const bool canApply = !name.empty() && !isTaken;
    ImGui::BeginDisabled(!canApply);
    if (ImGui::Button(isNew ? "Create" : "Save") || (isEntered && canApply)) {
      if (isNew) {
        unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("'{}' has unsaved changes.", name_), "Create", [this, name] {
          RevertUnsaved();
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

  void AnimationEditor::DrawPreviewTarget() {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preview on");
    ImGui::SameLine();
    if (ImGui::RadioButton("UI Layout", previewMode_ == PreviewMode::UI)) {
      previewMode_ = PreviewMode::UI;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("3D Model", previewMode_ == PreviewMode::Object3D)) {
      previewMode_ = PreviewMode::Object3D;
    }
    ImGui::SameLine();

    if (previewMode_ == PreviewMode::Object3D) {
      ImGui::SetNextItemWidth(240.0f);
      if (ImGui::BeginCombo("Model", previewModel_.empty() ? "(Cube)" : previewModel_.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("(Cube)", previewModel_.empty())) {
          previewModel_.clear();
          isModelDirty_ = true;
        }
        for (const std::string& model : ModelManager::GetInstance()->GetLoadedModelNames()) {
          if (ImGui::Selectable(model.c_str(), model == previewModel_)) {
            previewModel_ = model;
            isModelDirty_ = true;
          }
        }
        ImGui::EndCombo();
      }
      return;
    }

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("Layout", layoutName_.empty() ? "(none)" : layoutName_.c_str(), ImGuiComboFlags_HeightLarge)) {
      std::vector<std::string> sceneNames;
      for (const UILayout* layout : UILayout::GetInstances()) {
        const std::string& name = layout->GetName();
        if (layout == ownedLayout_.get() || name.empty() || std::ranges::find(sceneNames, name) != sceneNames.end()) {
          continue;
        }
        sceneNames.push_back(name);
        if (ImGui::Selectable(name.c_str(), name == layoutName_)) {
          pendingLayoutName_ = name;
        }
      }
      // シーンが読み込んでいないレイアウトは、エディタが読み込んでゲーム画面に重ねる
      ImGui::SeparatorText("Not in scene");
      for (const std::string& name : ListJsonFiles(UILayout::kDirectory)) {
        if (std::ranges::find(sceneNames, name) != sceneNames.end()) {
          continue;
        }
        if (ImGui::Selectable(name.c_str(), name == layoutName_)) {
          pendingLayoutName_ = name;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("The animation is previewed on this layout in the game view.");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("Element", boundTarget_.empty() ? "(Root)" : boundTarget_.c_str(), ImGuiComboFlags_HeightLarge)) {
      if (ImGui::Selectable("(Root)", boundTarget_.empty())) {
        boundTarget_.clear();
      }
      for (const std::string& name : ElementNames()) {
        if (ImGui::Selectable(name.c_str(), name == boundTarget_)) {
          boundTarget_ = name;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Tracks whose target is (Bound) move this element. (Root) moves the whole layout.");
  }

  void AnimationEditor::DrawPlaybackControls() {
    if (ImGui::Button(isPlaying_ ? "||" : ">", ImVec2(36, 0))) {
      TogglePlay();
    }
    ImGui::SetItemTooltip("Space");
    ImGui::SameLine();
    if (ImGui::Button("[]", ImVec2(36, 0))) {
      isPlaying_ = false;
      playhead_  = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("|<", ImVec2(36, 0))) {
      playhead_ = playElapsed_ = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(36, 0))) {
      playhead_ = playElapsed_ = clip_->GetDuration();
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Speed");
    for (float speed : kSpeedOptions) {
      ImGui::SameLine();
      if (ImGui::RadioButton(std::format("{}x", speed).c_str(), previewSpeed_ == speed)) {
        previewSpeed_ = speed;
      }
    }

    ImGui::SameLine();
    ImGui::Checkbox("Preview", &isPreviewEnabled_);
    ImGui::SetItemTooltip("While on, the target shows the pose at the playhead instead of what the game plays.");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::Button("Play on Target")) {
      PlayOnTarget();
    }
    ImGui::SetItemTooltip("Turns Preview off and plays the saved-or-edited clip with the real AnimationPlayer (delay and events included).");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::DragFloat("Delay", &playDelay_, 0.01f, 0.0f, 10.0f, "%.2f s");
    ImGui::SameLine();
    if (ImGui::Button("Stop on Target")) {
      StopOnTarget();
    }
  }

  void AnimationEditor::DrawAddTrackPopup() {
    if (!ImGui::BeginPopup(kAddTrackPopupId)) {
      return;
    }

    const bool isUI = previewMode_ == PreviewMode::UI;
    if (isUI) {
      ImGui::SetNextItemWidth(220.0f);
      if (ImGui::BeginCombo("Target", addTrackTarget_.empty() ? "(Bound)" : addTrackTarget_.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("(Bound)", addTrackTarget_.empty())) {
          addTrackTarget_.clear();
        }
        for (const std::string& name : ElementNames()) {
          if (ImGui::Selectable(name.c_str(), name == addTrackTarget_)) {
            addTrackTarget_ = name;
          }
        }
        ImGui::EndCombo();
      }
      ImGui::SetItemTooltip("(Bound) is the element chosen when the clip is played, so the clip can be reused on any element.");
    }
    // 3D は再生した Object3d 自身にしか効かないので、対象は常に Bound
    const std::string target = isUI ? addTrackTarget_ : std::string();

    using enum AnimProperty;
    bool isAdded = false;
    if (ImGui::Button("Position")) {
      isUI ? AddTracks(target, { TranslateX, TranslateY }) : AddTracks(target, { TranslateX, TranslateY, TranslateZ });
      isAdded = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotation")) {
      isUI ? AddTracks(target, { RotateZ }) : AddTracks(target, { RotateX, RotateY, RotateZ });
      isAdded = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Scale")) {
      isUI ? AddTracks(target, { ScaleX, ScaleY }) : AddTracks(target, { ScaleX, ScaleY, ScaleZ });
      isAdded = true;
    }
    if (ImGui::Button("Color")) {
      AddTracks(target, { ColorR, ColorG, ColorB });
      isAdded = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Alpha")) {
      AddTracks(target, { ColorA });
      isAdded = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Visible")) {
      AddTracks(target, { Visible });
      isAdded = true;
    }
    if (isUI) {
      ImGui::SameLine();
      if (ImGui::Button("Sprite Frame")) {
        AddTracks(target, { Frame });
        isAdded = true;
      }
      ImGui::SetItemTooltip("Splits the element's UV rect into columns x rows and shows one cell (set the grid in the inspector).");
    }

    if (isAdded) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  void AnimationEditor::DrawInspector() {
    std::vector<AnimTrack>& tracks     = clip_->GetTracks();
    const int               trackIndex = timeline_.GetSelectedTrack();

    ImGui::SeparatorText("Track");
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) {
      ImGui::TextDisabled(tracks.empty() ? "Add a track with + Track." : "Click a track label to select it.");
    }
    else {
      AnimTrack& track = tracks[trackIndex];
      if (previewMode_ == PreviewMode::UI || !track.target.empty()) {
        if (ImGui::BeginCombo("Target", track.target.empty() ? "(Bound)" : track.target.c_str(), ImGuiComboFlags_HeightLarge)) {
          if (ImGui::Selectable("(Bound)", track.target.empty())) {
            track.target.clear();
          }
          for (const std::string& name : ElementNames()) {
            if (ImGui::Selectable(name.c_str(), name == track.target)) {
              track.target = name;
            }
          }
          ImGui::EndCombo();
        }
        if (previewMode_ == PreviewMode::Object3D) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Object3d ignores tracks with a target. Set it to (Bound).");
        }
      }

      if (ImGui::BeginCombo("Property", GetAnimPropertyInfo(track.property).name, ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < static_cast<int>(AnimProperty::Count); ++i) {
          const AnimProperty property = static_cast<AnimProperty>(i);
          if (ImGui::Selectable(GetAnimPropertyInfo(property).name, property == track.property)) {
            track.property = property;
          }
        }
        ImGui::EndCombo();
      }

      if (track.property == AnimProperty::Frame) {
        if (ImGui::InputInt("Columns", &track.columns)) {
          track.columns = (std::max)(track.columns, 1);
        }
        if (ImGui::InputInt("Rows", &track.rows)) {
          track.rows = (std::max)(track.rows, 1);
        }
      }

      if (ImGui::Button("Delete Track")) {
        tracks.erase(tracks.begin() + trackIndex);
        timeline_.SetSelectedTrack(-1);
      }
    }

    DrawKeyInspector();
  }

  void AnimationEditor::DrawKeyInspector() {
    std::vector<AnimTrack>& tracks = clip_->GetTracks();

    std::vector<AnimKey*>   keys;
    std::vector<AnimEvent*> events;
    size_t                  firstTrack = 0;
    for (uint32_t id : timeline_.GetSelection()) {
      size_t trackIndex = 0;
      if (AnimKey* key = clip_->FindKey(id, &trackIndex)) {
        firstTrack = keys.empty() ? trackIndex : firstTrack;
        keys.push_back(key);
      }
      else if (AnimEvent* event = clip_->FindEvent(id)) {
        events.push_back(event);
      }
    }

    if (keys.empty() && events.empty()) {
      ImGui::SeparatorText("Key");
      ImGui::TextDisabled("Nothing selected. Double-click a row to add a key.");
      return;
    }
    if (!keys.empty()) {
      ImGui::SeparatorText("Key");
    }

    // 時刻を変えると並び替えでポインタの指す先が入れ替わるので、反映は最後にまとめる
    std::optional<float> newKeyTime;
    std::optional<float> newEventTime;

    if (keys.size() == 1) {
      AnimKey&         key   = *keys.front();
      const AnimTrack& track = tracks[firstTrack];
      ImGui::TextUnformatted(AnimationTimeline::TrackLabel(track).c_str());

      float time = key.time;
      if (ImGui::DragFloat("Time", &time, 0.005f, 0.0f, clip_->GetDuration(), "%.3f s")) {
        newKeyTime = std::clamp(timeline_.Snap(time), 0.0f, clip_->GetDuration());
      }

      if (track.property == AnimProperty::Visible) {
        bool isVisible = key.value >= 0.5f;
        if (ImGui::Checkbox("Visible", &isVisible)) {
          key.value = isVisible ? 1.0f : 0.0f;
        }
      }
      else if (track.property == AnimProperty::Frame) {
        int frame = static_cast<int>(key.value);
        if (ImGui::InputInt("Frame", &frame)) {
          key.value = static_cast<float>((std::max)(frame, 0));
        }
      }
      else {
        ImGui::DragFloat("Value", &key.value, track.property <= AnimProperty::TranslateZ ? 1.0f : 0.01f, 0.0f, 0.0f, std::format("%.3f {}", UnitOf(track.property)).c_str());
      }

      if (!IsStepProperty(track.property)) {
        EaseCombo("Ease", key.ease);
        ImGui::SetItemTooltip("How the value moves from this key to the next one.");
        if (key.ease == EaseType::CubicBezier) {
          if (ImGui::DragFloat2("Bezier P1", &key.bezierP1.x, 0.01f)) {
            key.bezierP1.x = std::clamp(key.bezierP1.x, 0.0f, 1.0f);
          }
          if (ImGui::DragFloat2("Bezier P2", &key.bezierP2.x, 0.01f)) {
            key.bezierP2.x = std::clamp(key.bezierP2.x, 0.0f, 1.0f);
          }
        }
        DrawEaseGraph(key);
      }
    }
    else if (keys.size() > 1) {
      ImGui::Text("%zu keys selected", keys.size());
      EaseType ease = keys.front()->ease;
      if (EaseCombo("Ease (all)", ease)) {
        for (AnimKey* key : keys) {
          key->ease = ease;
        }
      }
    }

    if (events.size() == 1) {
      ImGui::SeparatorText("Event");
      AnimEvent& event = *events.front();
      InputString("Name", event.name);
      ImGui::SetItemTooltip("Passed to PlayOptions::onEvent when playback passes this time.");
      float time = event.time;
      if (ImGui::DragFloat("Time##Event", &time, 0.005f, 0.0f, clip_->GetDuration(), "%.3f s")) {
        newEventTime = std::clamp(timeline_.Snap(time), 0.0f, clip_->GetDuration());
      }
    }
    else if (events.size() > 1) {
      ImGui::Text("%zu events selected", events.size());
    }

    if (newKeyTime) {
      keys.front()->time = *newKeyTime;
    }
    if (newEventTime) {
      events.front()->time = *newEventTime;
    }
    if (newKeyTime || newEventTime) {
      clip_->SortByTime();
    }

    ImGui::Spacing();
    if (ImGui::Button("Delete Selected", ImVec2(-1, 0))) {
      DeleteSelection();
    }
  }

  void AnimationEditor::DrawStatusBar() {
    size_t keyCount = 0;
    for (const AnimTrack& track : clip_->GetTracks()) {
      keyCount += track.keys.size();
    }
    if (isPreviewEnabled_) {
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.8f, 1.0f), "[PREVIEW]");
      ImGui::SameLine();
    }
    ImGui::Text("Tracks: %zu | Keys: %zu | Events: %zu | Selected: %zu",
      clip_->GetTracks().size(), keyCount, clip_->GetEvents().size(), timeline_.GetSelection().size());
  }

  bool AnimationEditor::Open(const std::string& name) {
    AnimationClip* clip = AnimationClip::Find(name);
    if (!clip) {
      return false;
    }
    clip_ = clip;
    name_ = name;
    timeline_.ClearSelection();
    timeline_.SetSelectedTrack(clip_->GetTracks().empty() ? -1 : 0);
    timeline_.RequestFit();
    playhead_    = 0.0f;
    playElapsed_ = 0.0f;
    isPlaying_   = false;
    tracker_.Reset();
    return true;
  }

  void AnimationEditor::RequestOpen(const std::string& name) {
    if (name == name_ && clip_) {
      return;
    }
    unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("'{}' has unsaved changes.", name_), "Switch", [this, name] {
      RevertUnsaved();
      if (!Open(name)) {
        LogToConsole(std::format("Animation Editor: failed to open '{}'", name), LogType::Error);
      }
    });
  }

  void AnimationEditor::RequestReload() {
    unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("'{}' has unsaved changes.", name_), "Reload", [this] {
      if (clip_ && clip_->Load()) {
        tracker_.Reset();
      }
      else {
        LogToConsole(std::format("Animation Editor: failed to reload '{}'", name_), LogType::Error);
      }
    });
  }

  void AnimationEditor::RevertUnsaved() {
    if (clip_ && tracker_.IsEdited()) {
      clip_->Load();
    }
  }

  void AnimationEditor::OpenNamePopup(NamePopup purpose) {
    namePopup_            = purpose;
    nameBuffer_[0]        = '\0';
    isNamePopupRequested_ = true;
  }

  bool AnimationEditor::Save() {
    if (!clip_ || !clip_->Save()) {
      LogToConsole(std::format("Animation Editor: failed to save '{}'", name_), LogType::Error);
      return false;
    }
    tracker_.MarkSaved();
    return true;
  }

  bool AnimationEditor::SaveCopy(const std::string& name) {
    if (!clip_) {
      return false;
    }
    AnimationClip* copy = AnimationClip::Create(name);
    copy->FromJson(clip_->ToJson(false));
    if (!copy->Save()) {
      LogToConsole(std::format("Animation Editor: failed to save a copy as '{}'", name), LogType::Error);
      return false;
    }
    return true;
  }

  bool AnimationEditor::CreateNew(const std::string& name) {
    if (!AnimationClip::Create(name)->Save() || !Open(name)) {
      LogToConsole(std::format("Animation Editor: failed to create '{}'", name), LogType::Error);
      return false;
    }
    return true;
  }

  bool AnimationEditor::IsNameTaken(const std::string& name) const {
    return !name.empty() && std::filesystem::exists(std::filesystem::path(AnimationClip::kDirectory) / (name + ".json"));
  }

  std::vector<std::string> AnimationEditor::ListJsonFiles(const char* directory) {
    std::vector<std::string> names;
    std::error_code          ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        names.push_back(entry.path().stem().string());
      }
    }
    std::ranges::sort(names);
    return names;
  }

  UILayout* AnimationEditor::FindSceneLayout(const std::string& name) const {
    if (name.empty()) {
      return nullptr;
    }
    for (UILayout* layout : UILayout::GetInstances()) {
      if (layout != ownedLayout_.get() && layout->GetName() == name) {
        return layout;
      }
    }
    return nullptr;
  }

  UILayout* AnimationEditor::FindTargetLayout() const {
    if (UILayout* layout = FindSceneLayout(layoutName_)) {
      return layout;
    }
    return ownedLayout_ && ownedLayout_->GetName() == layoutName_ ? ownedLayout_.get() : nullptr;
  }

  std::vector<std::string> AnimationEditor::ElementNames() const {
    std::vector<std::string> names;
    if (UILayout* layout = FindTargetLayout()) {
      CollectNames(layout->GetRoot(), names);
    }
    return names;
  }

  void AnimationEditor::UpdateLayoutPreview() {
    UILayout*  layout       = previewMode_ == PreviewMode::UI ? FindTargetLayout() : nullptr;
    const bool isPreviewing = layout && clip_ && isPreviewEnabled_;
    if (previewedLayout_ && previewedLayout_ != (isPreviewing ? layout : nullptr)) {
      ReleaseLayoutPreview();
    }
    if (!layout) {
      return;
    }
    if (isPreviewing) {
      layout->GetAnimator().SetPreview(clip_, boundTarget_, playhead_);
      previewedLayout_ = layout;
    }
    // ゲーム側が Update を呼ばない間（F7 停止中など）も反映する
    layout->Update();
  }

  void AnimationEditor::ReleaseLayoutPreview() {
    if (!previewedLayout_) {
      return;
    }
    // シーンの切り替えで消えていたら戻す先が無い
    if (std::ranges::find(UILayout::GetInstances(), previewedLayout_) != UILayout::GetInstances().end()) {
      previewedLayout_->GetAnimator().SetPreview(nullptr, "", 0.0f);
      previewedLayout_->Update();
    }
    previewedLayout_ = nullptr;
  }

  void AnimationEditor::UpdateObjectPreview() {
    if (previewMode_ != PreviewMode::Object3D) {
      previewObject_.reset();
      floorObject_.reset();
      return;
    }

    preview_.Update();

    if (!floorObject_) {
      floorObject_ = std::make_unique<Object3d>();
      floorObject_->Initialize();
      floorObject_->SetModel(PrimitiveBuilder::CreatePlane({ .width = 10.0f, .height = 10.0f }));
      floorObject_->SetMaterialColor(Vector4(0.35f, 0.35f, 0.35f, 1.0f));
      floorObject_->SetTranslate(Vector3(0.0f, -1.0f, 0.0f));
    }
    if (!previewObject_) {
      previewObject_ = std::make_unique<Object3d>();
      previewObject_->Initialize();
      isModelDirty_ = true;
    }
    if (isModelDirty_) {
      if (previewModel_.empty()) {
        previewObject_->SetModel(PrimitiveBuilder::CreateCube());
      }
      else {
        previewObject_->SetModel(previewModel_);
      }
      isModelDirty_ = false;
    }

    previewObject_->GetAnimator().SetPreview(isPreviewEnabled_ ? clip_ : nullptr, "", playhead_);
    floorObject_->Update();
    previewObject_->Update();
  }

  void AnimationEditor::AddTracks(const std::string& target, std::initializer_list<AnimProperty> properties) {
    std::vector<AnimTrack>& tracks = clip_->GetTracks();
    const float             time   = timeline_.Snap(playhead_);
    for (AnimProperty property : properties) {
      const bool exists = std::ranges::any_of(tracks, [&](const AnimTrack& track) { return track.target == target && track.property == property; });
      if (exists) {
        continue;
      }
      AnimTrack& track = tracks.emplace_back(AnimTrack{ .target = target, .property = property });
      track.keys.push_back(AnimKey{ .time = time, .value = GetAnimPropertyInfo(property).restValue, .ease = EaseType::OutCubic, .id = clip_->IssueId() });
    }
    timeline_.SetSelectedTrack(static_cast<int>(tracks.size()) - 1);
  }

  const char* AnimationEditor::UnitOf(AnimProperty property) const {
    switch (property) {
    case AnimProperty::TranslateX:
    case AnimProperty::TranslateY:
    case AnimProperty::TranslateZ:
      return previewMode_ == PreviewMode::UI ? "px" : "units";
    case AnimProperty::RotateX:
    case AnimProperty::RotateY:
    case AnimProperty::RotateZ:
      return "deg";
    default:
      return "x";
    }
  }

  void AnimationEditor::TogglePlay() {
    if (!clip_) {
      return;
    }
    if (!isPlaying_) {
      // 末尾で止まっていたら頭から再生する
      if (playhead_ >= clip_->GetDuration()) {
        playhead_ = 0.0f;
      }
      playElapsed_ = playhead_;
    }
    isPlaying_ = !isPlaying_;
  }

  void AnimationEditor::AddKeyAtPlayhead() {
    std::vector<AnimTrack>& tracks     = clip_->GetTracks();
    const int               trackIndex = timeline_.GetSelectedTrack();
    if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size())) {
      return;
    }
    AnimTrack&  track = tracks[trackIndex];
    const float time  = timeline_.Snap(playhead_);
    for (const AnimKey& key : track.keys) {
      if (std::abs(key.time - time) < 1e-4f) {
        timeline_.SetSelection({ key.id });
        return;
      }
    }
    const uint32_t id = clip_->IssueId();
    track.keys.push_back(AnimKey{ .time = time, .value = AnimationClip::EvaluateTrack(track, time), .ease = EaseType::OutCubic, .id = id });
    clip_->SortByTime();
    timeline_.SetSelection({ id });
  }

  void AnimationEditor::CopySelection() {
    clipboard_.clear();
    for (uint32_t id : timeline_.GetSelection()) {
      size_t trackIndex = 0;
      if (const AnimKey* key = clip_->FindKey(id, &trackIndex)) {
        clipboard_.push_back({ trackIndex, *key });
      }
    }
  }

  void AnimationEditor::Paste() {
    if (clipboard_.empty()) {
      return;
    }
    std::vector<AnimTrack>& tracks  = clip_->GetTracks();
    const float             minTime = std::ranges::min(clipboard_, {}, [](const CopiedKey& copied) { return copied.key.time; }).key.time;
    std::vector<uint32_t>   pasted;
    for (const CopiedKey& copied : clipboard_) {
      // 元のトラックが削除されていれば貼らない
      if (copied.track >= tracks.size()) {
        continue;
      }
      AnimKey key = copied.key;
      key.time    = std::clamp(timeline_.Snap(playhead_ + copied.key.time - minTime), 0.0f, clip_->GetDuration());
      key.id      = clip_->IssueId();
      tracks[copied.track].keys.push_back(key);
      pasted.push_back(key.id);
    }
    clip_->SortByTime();
    timeline_.SetSelection(std::move(pasted));
  }

  void AnimationEditor::DeleteSelection() {
    const std::vector<uint32_t>& selection  = timeline_.GetSelection();
    auto                         isSelected = [&](uint32_t id) { return std::ranges::find(selection, id) != selection.end(); };
    for (AnimTrack& track : clip_->GetTracks()) {
      std::erase_if(track.keys, [&](const AnimKey& key) { return isSelected(key.id); });
    }
    std::erase_if(clip_->GetEvents(), [&](const AnimEvent& event) { return isSelected(event.id); });
    timeline_.ClearSelection();
  }

  void AnimationEditor::PlayOnTarget() {
    if (!clip_) {
      return;
    }
    isPreviewEnabled_ = false;
    const AnimationPlayer::PlayOptions options{
      .delay   = playDelay_,
      .onEvent = [name = name_](const std::string& event) { LogToConsole(std::format("Animation '{}': event '{}'", name, event)); },
    };
    if (previewMode_ == PreviewMode::UI) {
      if (UILayout* layout = FindTargetLayout()) {
        layout->GetAnimator().Play(name_, boundTarget_, options);
      }
    }
    else if (previewObject_) {
      previewObject_->GetAnimator().Play(name_, "", options);
    }
  }

  void AnimationEditor::StopOnTarget() {
    if (previewMode_ == PreviewMode::UI) {
      if (UILayout* layout = FindTargetLayout()) {
        layout->GetAnimator().Stop(name_, boundTarget_);
      }
    }
    else if (previewObject_) {
      previewObject_->GetAnimator().Stop(name_);
    }
  }

} // namespace Tako

#endif // _DEBUG
