#include "UILayoutEditor.h"

#ifdef _DEBUG

#include "UILayout.h"
#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "TextureManager.h"
#include "EnginePaths.h"
#include "SpriteBasic.h"
#include "WinApp.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <limits>
#include <vector>

namespace Tako {

  namespace {

    constexpr const char* kElementPayload = "TAKO_UI_ELEMENT";

    constexpr ImU32 kGridColor      = IM_COL32(255, 255, 255, 40);
    constexpr ImU32 kSelectionColor = IM_COL32(255, 200, 0, 255);
    constexpr ImU32 kHandleColor    = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 kAnchorColor    = IM_COL32(80, 200, 255, 255);
    constexpr ImU32 kUvColor        = IM_COL32(255, 230, 0, 255);
    constexpr ImU32 kSliceColor     = IM_COL32(0, 255, 0, 255);

    // アンカープリセットの各軸の値。kStretch は親いっぱいの伸縮
    constexpr float       kStretch             = -1.0f;
    constexpr float       kPresetValues[4]     = { 0.0f, 0.5f, 1.0f, kStretch };
    constexpr const char* kPresetRowLabels[4]  = { "T", "M", "B", "S" };
    constexpr const char* kPresetColLabels[4]  = { "L", "C", "R", "S" };

    /// <summary>
    /// ゲーム画像（ImGui スクリーン座標）とクライアント px の対応
    /// </summary>
    struct ViewportMapping {
      ImVec2  imageMin;
      Vector2 scale;  ///< 画像 px ÷ クライアント px

      ImVec2 ToImage(const Vector2& p) const { return ImVec2(imageMin.x + p.x * scale.x, imageMin.y + p.y * scale.y); }
      Vector2 ToClient(const ImVec2& p) const { return { (p.x - imageMin.x) / scale.x, (p.y - imageMin.y) / scale.y }; }
    };

    Vector2 Mul(const Vector2& a, const Vector2& b) { return { a.x * b.x, a.y * b.y }; }

    void AddLog(const std::string& message, DebugUIManager::LogType type = DebugUIManager::LogType::Info) {
      DebugUIManager::GetInstance()->AddLog(message, type);
    }

    bool IsStretched(const UIElement& element) { return element.anchorMin != element.anchorMax; }

    bool Contains(const UIElement& element, const Vector2& p) {
      return p.x >= element.rectMin.x && p.y >= element.rectMin.y &&
        p.x <= element.rectMin.x + element.rectSize.x && p.y <= element.rectMin.y + element.rectSize.y;
    }

    size_t IndexInParent(const UIElement& element) {
      const auto& siblings = element.parent->children;
      const auto  it = std::ranges::find_if(siblings, [&](const std::unique_ptr<UIElement>& child) { return child.get() == &element; });
      return static_cast<size_t>(it - siblings.begin());
    }

    /// <summary>
    /// 角 index（0:左上 1:右上 2:右下 3:左下）の位置
    /// </summary>
    Vector2 Corner(const Vector2& rectMin, const Vector2& rectSize, int index) {
      return { rectMin.x + ((index == 1 || index == 2) ? rectSize.x : 0.0f), rectMin.y + (index >= 2 ? rectSize.y : 0.0f) };
    }

    Vector2 SnapPoint(const Vector2& p, float step) {
      if (step <= 0.0f) {
        return p;
      }
      return { std::round(p.x / step) * step, std::round(p.y / step) * step };
    }

    /// <summary>
    /// 描画順（前順）で、表示中かつスプライトを持つ要素を集める。グループは画面全体を覆いがちなので当たり判定に含めない。
    /// ロック中の要素は外すが、その子は対象に残す
    /// </summary>
    void CollectPickable(UIElement& node, std::vector<UIElement*>& out) {
      for (std::unique_ptr<UIElement>& child : node.children) {
        if (!child->visible) {
          continue;
        }
        if (child->sprite && !child->locked) {
          out.push_back(child.get());
        }
        CollectPickable(*child, out);
      }
    }

    std::string LayoutFilePath(const std::string& name) { return std::string(UILayout::kDirectory) + name + ".json"; }

    /// <summary>
    /// UILayout::kDirectory にあるレイアウト名（拡張子なし、昇順）
    /// </summary>
    std::vector<std::string> ListLayoutFiles() {
      std::vector<std::string> names;
      std::error_code          ec;
      for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(UILayout::kDirectory, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
          names.push_back(entry.path().stem().string());
        }
      }
      std::ranges::sort(names);
      return names;
    }

    /// <summary>
    /// ファイル名とゲームコードの文字列を兼ねるので、英数字と _ - だけ許す
    /// </summary>
    bool IsValidLayoutName(const std::string& name) {
      return !name.empty() && std::ranges::all_of(name, [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
    }

  } // namespace

  UILayoutEditor::UILayoutEditor() = default;
  UILayoutEditor::~UILayoutEditor() = default;

  void UILayoutEditor::Update() {
    SyncPreview();

    UILayout* layout = FindLayout();
    if (!layout) {
      pending_ = Pending{};
      return;
    }
    ApplyPending(*layout);

    // ゲーム側が Update を呼ばない間（F7 停止中など）も編集結果を反映する
    if (*isOpen_) {
      layout->Update();
    }
  }

  void UILayoutEditor::Draw() {
    if (!*isOpen_) {
      wantsKeyboard_     = false;
      isViewportFocused_ = false;
      dragMode_          = DragMode::None;
      return;
    }

    // 編集中のゲーム側レイアウトが無くなったら（シーン切り替えなど）、読み込み済みの最初のレイアウトを開く。
    // プレビュー中は開く対象を自分で選んでいるので切り替えない
    if (!isPreviewMode_ && !FindLayout()) {
      for (UILayout* layout : UILayout::GetInstances()) {
        if (layout != preview_.get() && !layout->GetName().empty()) {
          layoutName_ = layout->GetName();
          Select(nullptr);
          break;
        }
      }
    }

    bool isEditorFocused = false;
    ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("UI Editor", isOpen_)) {
      isEditorFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
      DrawToolbar(FindLayout());

      if (UILayout* layout = FindLayout()) {
        ImGui::Separator();
        ImGui::SetNextWindowSizeConstraints(ImVec2(140.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginChild("Hierarchy##UIEditor", ImVec2(kHierarchyPaneWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
          DrawHierarchy(*layout);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Inspector##UIEditor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
          if (UIElement* selected = FindSelected(*layout)) {
            DrawInspector(*layout, *selected);
          }
          else {
            ImGui::TextDisabled("Select an element in the hierarchy or the Game Viewport.");
          }
        }
        ImGui::EndChild();
      }
      else if (isPreviewMode_) {
        ImGui::TextDisabled("Opening %s ...", layoutName_.c_str());
      }
      else {
        ImGui::TextDisabled("No UILayout is loaded. Create one with New... or call UILayout::Load(\"Name\") in game code.");
      }
    }
    ImGui::End();

    UILayout*  layout   = FindLayout();
    UIElement* selected = layout ? FindSelected(*layout) : nullptr;
    wantsKeyboard_ = selected && !ImGui::GetIO().WantTextInput && (isEditorFocused || (isViewportFocused_ && isViewportEditEnabled_));
    if (wantsKeyboard_) {
      HandleArrowKeys(*selected);
    }
    isViewportFocused_ = false;
  }

  bool UILayoutEditor::DrawViewportOverlay(const ImVec2& imageMin, const ImVec2& imageMax) {
    isViewportFocused_ = ImGui::IsWindowFocused();
    if (!*isOpen_) {
      return false;
    }

    UILayout*    layout = FindLayout();
    const ImVec2 imageSize(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
    const float  clientWidth  = static_cast<float>(WinApp::clientWidth);
    const float  clientHeight = static_cast<float>(WinApp::clientHeight);
    if (!layout || imageSize.x <= 0.0f || imageSize.y <= 0.0f || clientWidth <= 0.0f || clientHeight <= 0.0f) {
      return false;
    }
    const ViewportMapping mapping{ imageMin, { imageSize.x / clientWidth, imageSize.y / clientHeight } };
    UIElement*            selected = FindSelected(*layout);

    // 直前のアイテム（ゲーム画像）へのテクスチャのドロップ。選択中がグループならその子、画像なら兄弟として置く
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowser::kTexturePayload)) {
        const UIElement* parent = selected ? (selected->texture.empty() ? selected : selected->parent) : nullptr;
        Request(PendingOp::AddTexture, (parent && parent->parent) ? parent->name : std::string(), {}, static_cast<const char*>(payload->Data));
        pending_.clientPos    = mapping.ToClient(ImGui::GetMousePos());
        pending_.hasClientPos = true;
      }
      ImGui::EndDragDropTarget();
    }

    bool isCaptured = false;
    if (isViewportEditEnabled_) {
      ImGui::SetCursorScreenPos(imageMin);
      ImGui::InvisibleButton("##UILayoutGizmo", imageSize);
      const bool    isHovered     = ImGui::IsItemHovered();
      const bool    isActive      = ImGui::IsItemActive();
      const bool    isActivated   = ImGui::IsItemActivated();
      const bool    isDeactivated = ImGui::IsItemDeactivated();
      const ImVec2  mouseImage    = ImGui::GetMousePos();
      const Vector2 mouse         = mapping.ToClient(mouseImage);

      if (isActivated) {
        dragMode_      = DragMode::None;
        hasDragged_    = false;
        isClickToPick_ = false;

        // 選択中の角ハンドルを最優先で掴む
        if (selected) {
          constexpr float kHitMargin = 2.0f;
          for (int i = 0; i < 4; ++i) {
            const ImVec2 corner = mapping.ToImage(Corner(selected->rectMin, selected->rectSize, i));
            if (std::abs(mouseImage.x - corner.x) <= kHandleHalfSize + kHitMargin && std::abs(mouseImage.y - corner.y) <= kHandleHalfSize + kHitMargin) {
              dragMode_   = DragMode::Resize;
              dragCorner_ = i;
              break;
            }
          }
        }

        // Unity の Rect Tool と同じく、選択中の矩形の中は選択中を掴む（子の画像や全画面の背景に選択を奪われない）。
        // ドラッグせずに離したときだけ、クリック位置の要素を選び直す
        if (dragMode_ == DragMode::None && selected && Contains(*selected, mouse)) {
          dragMode_      = DragMode::Move;
          isClickToPick_ = true;
        }

        if (dragMode_ == DragMode::None) {
          UIElement* picked = PickTopmost(*layout, mouse);
          Select(picked);
          selected = picked;
          if (selected) {
            dragMode_ = DragMode::Move;
          }
        }

        if (selected) {
          dragStartMouse_    = mouse;
          dragStartRectMin_  = selected->rectMin;
          dragStartRectSize_ = selected->rectSize;
        }
      }

      if (isActive && selected && dragMode_ != DragMode::None && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        hasDragged_ = true;
        UpdateDrag(*layout, *selected, mouse);
      }

      // 何も無い位置でのクリックなら選択はそのまま
      if (isDeactivated && isClickToPick_ && !hasDragged_) {
        if (UIElement* picked = PickTopmost(*layout, mouse)) {
          Select(picked);
          selected = picked;
        }
      }
      if (!isActive) {
        dragMode_      = DragMode::None;
        isClickToPick_ = false;
      }
      isCaptured = isHovered || isActive;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imageMin, imageMax, true);

    // グリッドはクライアント原点から gridSize_（基準解像度 px）間隔。スナップと同じ線
    if (isGridVisible_ && gridSize_ > 0.0f) {
      const float stepX = gridSize_ * layout->GetScale() * mapping.scale.x;
      const float stepY = gridSize_ * layout->GetScale() * mapping.scale.y;
      if (stepX >= kMinGridSpacing && stepY >= kMinGridSpacing) {
        for (int i = 0; imageMin.x + stepX * static_cast<float>(i) <= imageMax.x; ++i) {
          const float x = imageMin.x + stepX * static_cast<float>(i);
          drawList->AddLine(ImVec2(x, imageMin.y), ImVec2(x, imageMax.y), kGridColor);
        }
        for (int i = 0; imageMin.y + stepY * static_cast<float>(i) <= imageMax.y; ++i) {
          const float y = imageMin.y + stepY * static_cast<float>(i);
          drawList->AddLine(ImVec2(imageMin.x, y), ImVec2(imageMax.x, y), kGridColor);
        }
      }
    }

    // ponytail: 枠とハンドルは回転前の矩形。回転した要素の正確な枠が要るなら 4 隅を回転して描く
    if (selected) {
      drawList->AddRect(mapping.ToImage(selected->rectMin), mapping.ToImage(selected->rectMin + selected->rectSize), kSelectionColor, 0.0f, 0, 1.5f);

      // アンカー（親矩形上の基準）。伸縮する軸では Min と Max の 2 点に分かれる
      const UIElement& parent = *selected->parent;
      const Vector2    anchorCorners[4] = {
        selected->anchorMin,
        { selected->anchorMax.x, selected->anchorMin.y },
        selected->anchorMax,
        { selected->anchorMin.x, selected->anchorMax.y },
      };
      constexpr float kAnchorRadius = 5.0f;
      for (const Vector2& anchor : anchorCorners) {
        const ImVec2 p = mapping.ToImage(parent.rectMin + Mul(parent.rectSize, anchor));
        drawList->AddQuadFilled(ImVec2(p.x, p.y - kAnchorRadius), ImVec2(p.x + kAnchorRadius, p.y), ImVec2(p.x, p.y + kAnchorRadius), ImVec2(p.x - kAnchorRadius, p.y), kAnchorColor);
      }

      for (int i = 0; i < 4; ++i) {
        const ImVec2 c = mapping.ToImage(Corner(selected->rectMin, selected->rectSize, i));
        drawList->AddRectFilled(ImVec2(c.x - kHandleHalfSize, c.y - kHandleHalfSize), ImVec2(c.x + kHandleHalfSize, c.y + kHandleHalfSize), kHandleColor);
        drawList->AddRect(ImVec2(c.x - kHandleHalfSize, c.y - kHandleHalfSize), ImVec2(c.x + kHandleHalfSize, c.y + kHandleHalfSize), kSelectionColor);
      }

      drawList->AddCircle(mapping.ToImage(selected->rectMin + Mul(selected->rectSize, selected->pivot)), 4.0f, kSelectionColor, 0, 2.0f);
    }

    drawList->PopClipRect();
    return isCaptured;
  }

  void UILayoutEditor::DrawPreview() {
    // ゲーム側に同名がある間はシーンがそちらを描くので重ねない
    if (!*isOpen_ || !preview_ || FindLayout() != preview_.get()) {
      return;
    }
    SpriteBasic::GetInstance()->SetCommonRenderSetting();
    preview_->Draw();
  }

  UILayout* UILayoutEditor::FindLayout() const {
    if (UILayout* gameLayout = FindGameLayout(layoutName_)) {
      return gameLayout;
    }
    return (isPreviewMode_ && preview_ && preview_->GetName() == layoutName_) ? preview_.get() : nullptr;
  }

  UILayout* UILayoutEditor::FindGameLayout(const std::string& name) const {
    if (name.empty()) {
      return nullptr;
    }
    for (UILayout* layout : UILayout::GetInstances()) {
      if (layout != preview_.get() && layout->GetName() == name) {
        return layout;
      }
    }
    return nullptr;
  }

  void UILayoutEditor::SyncPreview() {
    if (!pendingNewLayout_.empty()) {
      preview_ = std::make_unique<UILayout>();
      preview_->Create(pendingNewLayout_);
      const std::string path = LayoutFilePath(pendingNewLayout_);
      if (preview_->Save()) {
        AddLog("UILayout created: " + path);
      }
      else {
        AddLog("UILayout create failed: " + path, DebugUIManager::LogType::Error);
      }
      layoutName_    = pendingNewLayout_;
      isPreviewMode_ = true;
      Select(nullptr);
      pendingNewLayout_.clear();
    }

    // シーンが同名のレイアウトを読み込んだら、そちらを編集する
    if (FindGameLayout(layoutName_)) {
      isPreviewMode_ = false;
    }

    // ponytail: 切り替えると未保存のプレビュー編集は破棄される。残したくなったら変更フラグで確認ダイアログを出す
    if (preview_ && (!isPreviewMode_ || preview_->GetName() != layoutName_)) {
      preview_.reset();
    }
    if (isPreviewMode_ && !preview_) {
      preview_ = std::make_unique<UILayout>();
      preview_->Load(layoutName_);
    }
  }

  UIElement* UILayoutEditor::FindSelected(UILayout& layout) const {
    return selectedName_.empty() ? nullptr : layout.Find(selectedName_);
  }

  void UILayoutEditor::Select(const UIElement* element) {
    selectedName_     = element ? element->name : std::string();
    isRenaming_       = false;
    isRenameRejected_ = false;
  }

  void UILayoutEditor::DrawToolbar(UILayout* layout) {
    auto open = [this](const std::string& name, bool isPreview) {
      if (name != layoutName_ || isPreview != isPreviewMode_) {
        layoutName_    = name;
        isPreviewMode_ = isPreview;
        Select(nullptr);
      }
    };

    const std::string comboLabel = layoutName_.empty() ? "(none)" : (isPreviewMode_ ? layoutName_ + " (preview)" : layoutName_);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("Layout", comboLabel.c_str())) {
      std::vector<std::string> gameNames;
      for (UILayout* candidate : UILayout::GetInstances()) {
        const std::string& name = candidate->GetName();
        if (candidate == preview_.get() || name.empty()) {
          continue;
        }
        gameNames.push_back(name);
        ImGui::PushID(candidate);
        if (ImGui::Selectable(name.c_str(), !isPreviewMode_ && name == layoutName_)) {
          open(name, false);
        }
        ImGui::PopID();
      }

      // 今のシーンが読み込んでいないファイルは、エディタ自前のインスタンスで開く
      ImGui::SeparatorText("Not in scene");
      for (const std::string& name : ListLayoutFiles()) {
        if (std::ranges::find(gameNames, name) != gameNames.end()) {
          continue;
        }
        const std::string label = name + " (preview)";
        if (ImGui::Selectable(label.c_str(), isPreviewMode_ && name == layoutName_)) {
          open(name, true);
        }
      }
      ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("New...")) {
      newLayoutBuffer_[0] = '\0';
      newLayoutError_.clear();
      ImGui::OpenPopup("New UI Layout");
    }
    DrawNewLayoutPopup();

    ImGui::BeginDisabled(!layout);
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      const std::string path = LayoutFilePath(layout->GetName());
      if (layout->Save()) {
        AddLog("UILayout saved: " + path);
      }
      else {
        AddLog("UILayout save failed: " + path, DebugUIManager::LogType::Error);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      Request(PendingOp::Reload, {});
    }
    ImGui::EndDisabled();

    ImGui::Checkbox("Viewport Edit", &isViewportEditEnabled_);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &isGridVisible_);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &isSnapEnabled_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("Grid Size", &gridSize_, 1.0f, 1.0f, 512.0f, "%.0f px");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip(
      "Viewport: click to select, drag to move, drag corners to resize.\n"
      "Dragging inside the selected element moves it, even over its children.\n"
      "Ctrl while dragging toggles Snap. Arrow keys move 1 px (Shift: Grid Size).\n"
      "Drop textures from Assets onto the viewport, the hierarchy or the Texture field.");

    if (layout && layout == preview_.get()) {
      const std::string code = std::format("layout.Load(\"{}\");", layout->GetName());
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Preview (not loaded by the current scene). Use in code: %s", code.c_str());
      ImGui::SameLine();
      if (ImGui::SmallButton("Copy")) {
        ImGui::SetClipboardText(code.c_str());
      }
    }
  }

  void UILayoutEditor::DrawNewLayoutPopup() {
    if (!ImGui::BeginPopup("New UI Layout")) {
      return;
    }

    ImGui::Text("File: %s<Name>.json", UILayout::kDirectory);
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool isEntered = ImGui::InputText("Name", newLayoutBuffer_, sizeof(newLayoutBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::Button("Create") || isEntered) {
      const std::string name = newLayoutBuffer_;
      if (!IsValidLayoutName(name)) {
        newLayoutError_ = "Use only letters, digits, '_' and '-'.";
      }
      else if (std::filesystem::exists(LayoutFilePath(name)) || FindGameLayout(name)) {
        newLayoutError_ = "A layout with this name already exists.";
      }
      else {
        // 古いプレビューの破棄（スプライト解放）を伴うので、作成は次の Update で行う
        pendingNewLayout_ = name;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    if (!newLayoutError_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", newLayoutError_.c_str());
    }

    ImGui::EndPopup();
  }

  void UILayoutEditor::DrawHierarchy(UILayout& layout) {
    for (std::unique_ptr<UIElement>& child : layout.GetRoot().children) {
      DrawTreeNode(*child);
    }

    // 残りの空き領域: クリックで選択解除、ドロップでルート直下へ、右クリックで追加
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##HierarchyBlank", ImVec2((std::max)(avail.x, 1.0f), (std::max)(avail.y, ImGui::GetFrameHeight())));
    if (ImGui::IsItemClicked()) {
      Select(nullptr);
    }
    AcceptHierarchyDrop({});
    if (ImGui::BeginPopupContextItem("##HierarchyBlankMenu")) {
      if (ImGui::MenuItem("Add Image")) {
        Request(PendingOp::AddImage, {});
      }
      if (ImGui::MenuItem("Add Group")) {
        Request(PendingOp::AddGroup, {});
      }
      ImGui::EndPopup();
    }
  }

  void UILayoutEditor::DrawTreeNode(UIElement& element) {
    // 右端のロックボタンを重ねるため AllowOverlap
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth |
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap;
    if (element.children.empty()) {
      flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (element.name == selectedName_) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(&element);
    const float rowRightX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (!element.visible) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    const bool isOpen = ImGui::TreeNodeEx("##Node", flags, "%s%s", element.name.c_str(), element.texture.empty() ? "  [Group]" : "");
    if (!element.visible) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      Select(&element);
    }

    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload(kElementPayload, element.name.c_str(), element.name.size() + 1);
      ImGui::TextUnformatted(element.name.c_str());
      ImGui::EndDragDropSource();
    }
    AcceptHierarchyDrop(element.name);

    if (ImGui::BeginPopupContextItem()) {
      Select(&element);
      const size_t index = IndexInParent(element);
      if (ImGui::MenuItem("Add Image")) {
        Request(PendingOp::AddImage, element.name);
      }
      if (ImGui::MenuItem("Add Group")) {
        Request(PendingOp::AddGroup, element.name);
      }
      if (ImGui::MenuItem("Duplicate")) {
        Request(PendingOp::Duplicate, element.name);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Move Up", nullptr, false, index > 0)) {
        Request(PendingOp::MoveUp, element.name);
      }
      if (ImGui::MenuItem("Move Down", nullptr, false, index + 1 < element.parent->children.size())) {
        Request(PendingOp::MoveDown, element.name);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Delete")) {
        Request(PendingOp::Remove, element.name);
      }
      ImGui::EndPopup();
    }

    // 行の右端にロックの切り替え。既定フォントに錠前のアイコン文字が無いので "L" で表す
    const float lockButtonWidth = ImGui::CalcTextSize("L").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(rowRightX - lockButtonWidth, ImGui::GetCursorScreenPos().y));
    ImGui::PushStyleColor(ImGuiCol_Button, element.locked ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(element.locked ? ImGuiCol_Text : ImGuiCol_TextDisabled));
    if (ImGui::SmallButton("L")) {
      element.locked = !element.locked;
    }
    ImGui::PopStyleColor(2);
    ImGui::SetItemTooltip(element.locked ? "Locked: not selectable by clicking in the Game Viewport" : "Lock to prevent selecting by clicking in the Game Viewport");

    if (isOpen) {
      for (std::unique_ptr<UIElement>& child : element.children) {
        DrawTreeNode(*child);
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }

  void UILayoutEditor::AcceptHierarchyDrop(const std::string& parentName) {
    if (!ImGui::BeginDragDropTarget()) {
      return;
    }
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kElementPayload)) {
      Request(PendingOp::Reparent, static_cast<const char*>(payload->Data), parentName);
    }
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowser::kTexturePayload)) {
      Request(PendingOp::AddTexture, parentName, {}, static_cast<const char*>(payload->Data));
    }
    ImGui::EndDragDropTarget();
  }

  void UILayoutEditor::DrawInspector(UILayout& layout, UIElement& element) {
    // 要素ごとに ID を分け、別要素の選択中に前の要素の入力確定が流れ込まないようにする
    ImGui::PushID(&element);

    // 名前は確定時に、空または重複なら取り消す
    if (!isRenaming_) {
      strncpy_s(renameBuffer_, element.name.c_str(), _TRUNCATE);
    }
    ImGui::InputText("Name", renameBuffer_, sizeof(renameBuffer_));
    isRenaming_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      const std::string newName = renameBuffer_;
      isRenameRejected_ = newName.empty() || (newName != element.name && layout.Find(newName));
      if (!isRenameRejected_) {
        element.name  = newName;
        selectedName_ = newName;
      }
    }
    if (isRenameRejected_) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Name must be non-empty and unique in this layout.");
    }
    ImGui::Checkbox("Visible", &element.visible);
    ImGui::SameLine();
    ImGui::Checkbox("Locked", &element.locked);
    ImGui::SetItemTooltip("Editor only: a locked element is not selected by clicking in the Game Viewport.");

    ImGui::SeparatorText("Texture");
    if (ImGui::BeginCombo("Texture", element.texture.empty() ? "(none)" : element.texture.c_str())) {
      if (ImGui::Selectable("(none)", element.texture.empty())) {
        Request(PendingOp::SetTexture, element.name, {}, {});
      }
      for (const std::string& name : TextureManager::GetInstance()->GetLoadedTextureFileNames()) {
        if (ImGui::Selectable(name.c_str(), name == element.texture)) {
          Request(PendingOp::SetTexture, element.name, {}, name);
        }
      }
      ImGui::EndCombo();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowser::kTexturePayload)) {
        Request(PendingOp::SetTexture, element.name, {}, static_cast<const char*>(payload->Data));
      }
      ImGui::EndDragDropTarget();
    }
    if (!element.texture.empty() && !element.sprite) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Texture not found");
    }

    ImGui::SeparatorText("Rect");
    DrawAnchorPresets(layout, element);

    // アンカーとピボットを数値で変えても画面上の矩形は保つ（Unity の Inspector と同じ）
    const Vector2 rectMin  = element.rectMin;
    const Vector2 rectSize = element.rectSize;
    bool          isAnchorEdited = false;
    isAnchorEdited |= ImGui::DragFloat2("Anchor Min", &element.anchorMin.x, 0.01f, 0.0f, 1.0f);
    isAnchorEdited |= ImGui::DragFloat2("Anchor Max", &element.anchorMax.x, 0.01f, 0.0f, 1.0f);
    isAnchorEdited |= ImGui::DragFloat2("Pivot", &element.pivot.x, 0.01f, 0.0f, 1.0f);
    if (isAnchorEdited) {
      element.anchorMax = { (std::max)(element.anchorMin.x, element.anchorMax.x), (std::max)(element.anchorMin.y, element.anchorMax.y) };
      layout.SetScreenRect(element, rectMin, rectSize);
    }
    ImGui::DragFloat2("Position", &element.position.x, 1.0f);
    ImGui::DragFloat2(IsStretched(element) ? "Size Delta###Size" : "Size###Size", &element.size.x, 1.0f);
    ImGui::DragFloat("Rotation", &element.rotation, 0.5f, -360.0f, 360.0f, "%.1f deg");

    ImGui::SeparatorText("Appearance");
    ImGui::ColorEdit4("Color", &element.color.x);
    ImGui::Checkbox("Flip X", &element.flipX);
    ImGui::SameLine();
    ImGui::Checkbox("Flip Y", &element.flipY);

    if (element.sprite) {
      ImGui::DragFloat2("UV Left Top", &element.uvLeftTop.x, 1.0f, 0.0f, 8192.0f, "%.0f");
      ImGui::DragFloat2("UV Size", &element.uvSize.x, 1.0f, 0.0f, 8192.0f, "%.0f");
      if (ImGui::Button("Fit UV")) {
        const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(element.texture);
        element.uvLeftTop = {};
        element.uvSize    = { static_cast<float>(metadata.width), static_cast<float>(metadata.height) };
      }
      ImGui::SameLine();
      // Unity の Set Native Size。伸縮する軸はアンカー幅で決まるので対象外
      if (ImGui::Button("Set Native Size")) {
        if (element.anchorMin.x == element.anchorMax.x) {
          element.size.x = element.uvSize.x;
        }
        if (element.anchorMin.y == element.anchorMax.y) {
          element.size.y = element.uvSize.y;
        }
      }
      ImGui::DragFloat4("Slice Border", &element.sliceBorder.x, 1.0f, 0.0f, 4096.0f, "%.0f");
      ImGui::SetItemTooltip("9-slice border in texture px (Left, Top, Right, Bottom). Corners keep their size.");
      DrawTexturePreview(element);
    }

    ImGui::PopID();
  }

  void UILayoutEditor::DrawAnchorPresets(UILayout& layout, UIElement& element) {
    ImGui::TextDisabled("Anchor Presets (Shift: also pivot, Alt: also position)");
    const float buttonSize = ImGui::GetFrameHeight() * 1.2f;

    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        const float   valueX    = kPresetValues[col];
        const float   valueY    = kPresetValues[row];
        const bool    isStretchX = valueX == kStretch;
        const bool    isStretchY = valueY == kStretch;
        const Vector2 presetMin = { isStretchX ? 0.0f : valueX, isStretchY ? 0.0f : valueY };
        const Vector2 presetMax = { isStretchX ? 1.0f : valueX, isStretchY ? 1.0f : valueY };
        const bool    isCurrent = element.anchorMin == presetMin && element.anchorMax == presetMax;

        if (col > 0) {
          ImGui::SameLine();
        }
        ImGui::PushID(row * 4 + col);
        if (isCurrent) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        const std::string label   = std::string(kPresetRowLabels[row]) + kPresetColLabels[col];
        const bool        clicked = ImGui::Button(label.c_str(), ImVec2(buttonSize, buttonSize));
        if (isCurrent) {
          ImGui::PopStyleColor();
        }
        ImGui::PopID();
        if (!clicked) {
          continue;
        }

        const ImGuiIO& io       = ImGui::GetIO();
        const Vector2  rectMin  = element.rectMin;
        const Vector2  rectSize = element.rectSize;
        element.anchorMin = presetMin;
        element.anchorMax = presetMax;
        if (io.KeyShift) {
          element.pivot = { isStretchX ? 0.5f : valueX, isStretchY ? 0.5f : valueY };
        }
        layout.SetScreenRect(element, rectMin, rectSize);
        if (io.KeyAlt) {
          // アンカー位置へ寄せる。伸縮する軸は親いっぱいに広げる
          element.position = {};
          if (isStretchX) {
            element.size.x = 0.0f;
          }
          if (isStretchY) {
            element.size.y = 0.0f;
          }
        }
      }
    }
  }

  void UILayoutEditor::DrawTexturePreview(const UIElement& element) {
    TextureManager* textureManager = TextureManager::GetInstance();
    if (!textureManager->IsLoaded(element.texture)) {
      return;
    }
    // ImGui のシェーダーは Texture2D 前提
    const DirectX::TexMetadata& metadata = textureManager->GetMetaData(element.texture);
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.IsCubemap()) {
      return;
    }

    const float texWidth  = static_cast<float>(metadata.width);
    const float texHeight = static_cast<float>(metadata.height);
    const float scale     = (std::min)(ImGui::GetContentRegionAvail().x / texWidth, kPreviewMaxHeight / texHeight);
    if (scale <= 0.0f) {
      return;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(textureManager->GetSRVGPUHandle(element.texture).ptr), ImVec2(texWidth * scale, texHeight * scale));

    ImDrawList* drawList  = ImGui::GetWindowDrawList();
    auto        toPreview = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    const Vector2 uvMin   = element.uvLeftTop;
    const Vector2 uvMax   = element.uvLeftTop + element.uvSize;
    const Vector4& border = element.sliceBorder;

    drawList->AddRect(toPreview(uvMin.x, uvMin.y), toPreview(uvMax.x, uvMax.y), kUvColor);
    if (border.x > 0.0f) {
      drawList->AddLine(toPreview(uvMin.x + border.x, uvMin.y), toPreview(uvMin.x + border.x, uvMax.y), kSliceColor);
    }
    if (border.z > 0.0f) {
      drawList->AddLine(toPreview(uvMax.x - border.z, uvMin.y), toPreview(uvMax.x - border.z, uvMax.y), kSliceColor);
    }
    if (border.y > 0.0f) {
      drawList->AddLine(toPreview(uvMin.x, uvMin.y + border.y), toPreview(uvMax.x, uvMin.y + border.y), kSliceColor);
    }
    if (border.w > 0.0f) {
      drawList->AddLine(toPreview(uvMin.x, uvMax.y - border.w), toPreview(uvMax.x, uvMax.y - border.w), kSliceColor);
    }
  }

  void UILayoutEditor::Request(PendingOp op, const std::string& target, const std::string& newParent, const std::string& texture) {
    pending_ = Pending{ .op = op, .target = target, .newParent = newParent, .texture = texture };
  }

  void UILayoutEditor::ApplyPending(UILayout& layout) {
    const Pending pending = std::move(pending_);
    pending_ = Pending{};
    if (pending.op == PendingOp::None) {
      return;
    }

    UIElement* target = pending.target.empty() ? &layout.GetRoot() : layout.Find(pending.target);
    if (!target) {
      return;
    }
    const bool isRoot = target == &layout.GetRoot();

    switch (pending.op) {
    case PendingOp::AddImage: {
      UIElement* element = layout.AddChild(*target, "Image");
      layout.SetTexture(*element, EnginePaths::TexturePath("white.dds"));
      Select(element);
      break;
    }
    case PendingOp::AddGroup: {
      // 子のアンカーはこの矩形が基準になるので、既定は親いっぱい
      UIElement* element = layout.AddChild(*target, "Group");
      element->anchorMin = { 0.0f, 0.0f };
      element->anchorMax = { 1.0f, 1.0f };
      element->size      = {};
      Select(element);
      break;
    }
    case PendingOp::AddTexture: {
      const Vector2 center = pending.hasClientPos ? pending.clientPos : target->rectMin + target->rectSize * 0.5f;
      CreateFromTexture(layout, *target, pending.texture, center);
      break;
    }
    case PendingOp::SetTexture:
      if (!isRoot) {
        layout.SetTexture(*target, pending.texture);
      }
      break;
    case PendingOp::Duplicate:
      if (!isRoot) {
        Select(layout.Duplicate(*target));
      }
      break;
    case PendingOp::Remove:
      if (!isRoot) {
        layout.Remove(*target);
        if (!FindSelected(layout)) {
          Select(nullptr);
        }
      }
      break;
    case PendingOp::MoveUp:
    case PendingOp::MoveDown:
      if (!isRoot) {
        const size_t index = IndexInParent(*target);
        if (pending.op == PendingOp::MoveDown) {
          layout.MoveTo(*target, *target->parent, index + 1);
        }
        else if (index > 0) {
          layout.MoveTo(*target, *target->parent, index - 1);
        }
      }
      break;
    case PendingOp::Reparent: {
      UIElement* newParent = pending.newParent.empty() ? &layout.GetRoot() : layout.Find(pending.newParent);
      if (!isRoot && newParent) {
        layout.MoveTo(*target, *newParent, (std::numeric_limits<size_t>::max)());
      }
      break;
    }
    case PendingOp::Reload:
      layout.Load(layout.GetName());
      dragMode_ = DragMode::None;
      AddLog("UILayout reloaded: " + layout.GetName());
      break;
    case PendingOp::None:
      break;
    }
  }

  void UILayoutEditor::CreateFromTexture(UILayout& layout, UIElement& parent, const std::string& texture, const Vector2& clientCenter) {
    const std::string stem    = std::filesystem::path(texture).stem().string();
    UIElement*        element = layout.AddChild(parent, stem.empty() ? "Image" : stem);
    layout.SetTexture(*element, texture);
    if (!element->sprite) {
      // 読み込めなかった（SetTexture が警告済み）ので空の要素を残さない
      layout.Remove(*element);
      return;
    }

    // テクスチャ px をそのまま基準解像度 px とみなした原寸
    const Vector2 size = element->uvSize * layout.GetScale();
    layout.SetScreenRect(*element, clientCenter - size * 0.5f, size);
    Select(element);
  }

  UIElement* UILayoutEditor::PickTopmost(UILayout& layout, const Vector2& clientPos) const {
    std::vector<UIElement*> pickable;
    CollectPickable(layout.GetRoot(), pickable);
    for (auto it = pickable.rbegin(); it != pickable.rend(); ++it) {
      if (Contains(**it, clientPos)) {
        return *it;
      }
    }
    return nullptr;
  }

  void UILayoutEditor::UpdateDrag(UILayout& layout, UIElement& element, const Vector2& mouse) {
    const Vector2 delta = mouse - dragStartMouse_;

    // Ctrl を押している間はスナップの ON/OFF を反転する。グリッドは画面空間で丸め、描いた線と一致させる
    const bool  isSnapping = isSnapEnabled_ != ImGui::GetIO().KeyCtrl;
    const float step       = isSnapping ? gridSize_ * layout.GetScale() : 0.0f;

    if (dragMode_ == DragMode::Move) {
      layout.SetScreenRect(element, SnapPoint(dragStartRectMin_ + delta, step), dragStartRectSize_);
      return;
    }

    // 掴んだ角だけを動かし、対角は固定する
    Vector2       rectMin  = dragStartRectMin_;
    Vector2       rectMax  = dragStartRectMin_ + dragStartRectSize_;
    const bool    isRight  = dragCorner_ == 1 || dragCorner_ == 2;
    const bool    isBottom = dragCorner_ >= 2;
    const Vector2 corner   = SnapPoint(Corner(dragStartRectMin_, dragStartRectSize_, dragCorner_) + delta, step);
    if (isRight) {
      rectMax.x = (std::max)(corner.x, rectMin.x);
    }
    else {
      rectMin.x = (std::min)(corner.x, rectMax.x);
    }
    if (isBottom) {
      rectMax.y = (std::max)(corner.y, rectMin.y);
    }
    else {
      rectMin.y = (std::min)(corner.y, rectMax.y);
    }
    layout.SetScreenRect(element, rectMin, rectMax - rectMin);
  }

  void UILayoutEditor::HandleArrowKeys(UIElement& element) const {
    // position は基準解像度 px なので、画面サイズに関係なく 1 単位ずつ動く
    const float step = ImGui::GetIO().KeyShift ? gridSize_ : 1.0f;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
      element.position.x -= step;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
      element.position.x += step;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
      element.position.y -= step;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
      element.position.y += step;
    }
  }

} // namespace Tako

#endif // _DEBUG
