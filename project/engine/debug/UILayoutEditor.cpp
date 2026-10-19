#include "UILayoutEditor.h"

#ifdef _DEBUG

#include "UILayout.h"
#include "Sprite.h"
#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "TextureManager.h"
#include "EnginePaths.h"
#include "SpriteBasic.h"
#include "UnsavedChangesModal.h"
#include "WinApp.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <limits>
#include <utility>
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
    constexpr ImU32 kTextureColor   = IM_COL32(128, 128, 128, 255);

    constexpr float kHitMargin = 2.0f;  ///< ハンドルや線を掴める距離の、描画の半径への上乗せ（px）

    // HitEdges が返す辺の bit
    constexpr int kEdgeLeft   = 1;
    constexpr int kEdgeTop    = 2;
    constexpr int kEdgeRight  = 4;
    constexpr int kEdgeBottom = 8;
    constexpr int kEdgeAll    = kEdgeLeft | kEdgeTop | kEdgeRight | kEdgeBottom;

    constexpr ImVec4 kWarningColor    = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    constexpr ImU32  kRegionFillColor = IM_COL32(0, 255, 0, 40);
    constexpr ImU32  kResultLineColor = IM_COL32(255, 255, 255, 60);
    constexpr ImU32  kLabelBackColor  = IM_COL32(0, 0, 0, 160);

    // 9-slice の区画（UISlice の並び）
    constexpr int         kCenterRegion                        = 4;
    constexpr const char* kRegionLabels[UISlice::kRegionCount] = { "TL", "T", "TR", "L", "C", "R", "BL", "B", "BR" };
    constexpr const char* kRegionNames[UISlice::kRegionCount]  = { "Top Left", "Top", "Top Right", "Left", "Center", "Right", "Bottom Left", "Bottom", "Bottom Right" };

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
    /// 縦 2 本・横 2 本の線のうち、mouse から radius 以内にあるもの（縦と横それぞれ近いほうの 1 本）の辺の bit
    /// </summary>
    /// <param name="mouse">マウス（スクリーン座標）</param>
    /// <param name="lines">線の位置（スクリーン座標、x=左 y=上 z=右 w=下）</param>
    /// <param name="spanMin">線を引く範囲の左上。この範囲の外では当たらない</param>
    /// <param name="spanMax">線を引く範囲の右下</param>
    /// <param name="radius">当たりとする距離（px）</param>
    /// <returns>kEdgeLeft などの和。どれにも当たらなければ 0</returns>
    int HitEdges(const ImVec2& mouse, const Vector4& lines, const ImVec2& spanMin, const ImVec2& spanMax, float radius) {
      int edges = 0;
      // 2 本が重なっているときは、マウスのある側の線を掴む（その向きにしか動かせないため）
      if (mouse.y >= spanMin.y - radius && mouse.y <= spanMax.y + radius) {
        const float left  = std::abs(mouse.x - lines.x);
        const float right = std::abs(mouse.x - lines.z);
        if ((std::min)(left, right) <= radius) {
          edges |= (left < right || (left == right && mouse.x < lines.x)) ? kEdgeLeft : kEdgeRight;
        }
      }
      if (mouse.x >= spanMin.x - radius && mouse.x <= spanMax.x + radius) {
        const float top    = std::abs(mouse.y - lines.y);
        const float bottom = std::abs(mouse.y - lines.w);
        if ((std::min)(top, bottom) <= radius) {
          edges |= (top < bottom || (top == bottom && mouse.y < lines.y)) ? kEdgeTop : kEdgeBottom;
        }
      }
      return edges;
    }

    /// <summary>
    /// HitEdges の結果に合うマウスカーソル
    /// </summary>
    ImGuiMouseCursor EdgeCursor(int edges) {
      if (edges == kEdgeAll) {
        return ImGuiMouseCursor_ResizeAll;
      }
      const bool isHorizontal = (edges & (kEdgeLeft | kEdgeRight)) != 0;
      const bool isVertical   = (edges & (kEdgeTop | kEdgeBottom)) != 0;
      if (isHorizontal && isVertical) {
        // 左上・右下の角は ＼、右上・左下の角は ／
        return ((edges & kEdgeLeft) != 0) == ((edges & kEdgeTop) != 0) ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW;
      }
      return isHorizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
    }

    /// <summary>
    /// 9-slice の区画の範囲（テクスチャ px）。枠は切り出し範囲の各辺から測る
    /// </summary>
    /// <returns>左上と右下</returns>
    std::pair<Vector2, Vector2> RegionRect(const UIElement& element, const Vector4& border, int region) {
      const Vector2 uvMin = element.uvLeftTop;
      const Vector2 uvMax = element.uvLeftTop + element.uvSize;
      const float   xs[4] = { uvMin.x, uvMin.x + border.x, uvMax.x - border.z, uvMax.x };
      const float   ys[4] = { uvMin.y, uvMin.y + border.y, uvMax.y - border.w, uvMax.y };
      const int     col   = region % 3;
      const int     row   = region / 3;
      return { { xs[col], ys[row] }, { xs[col + 1], ys[row + 1] } };
    }

    /// <summary>
    /// 左右・上下の枠を、変わった側の値にそろえる。合計が切り出し範囲を超えないよう半分で止める。変わっていない組はそのまま
    /// </summary>
    void MirrorBorder(Vector4& border, const Vector4& before, const Vector2& uvSize) {
      if (border.x != before.x || border.z != before.z) {
        const float value = (std::min)(border.x != before.x ? border.x : border.z, (std::max)(uvSize.x * 0.5f, 0.0f));
        border.x = value;
        border.z = value;
      }
      if (border.y != before.y || border.w != before.w) {
        const float value = (std::min)(border.y != before.y ? border.y : border.w, (std::max)(uvSize.y * 0.5f, 0.0f));
        border.y = value;
        border.w = value;
      }
    }

    /// <summary>
    /// node の子孫のうち、texture を 9-slice で使う要素を描画順に集める（9-slice を上書きしている要素も含む）
    /// </summary>
    void CollectTextureUsers(UIElement& node, const std::string& texture, std::vector<UIElement*>& out) {
      for (std::unique_ptr<UIElement>& child : node.children) {
        if (child->isSliced && child->texture == texture) {
          out.push_back(child.get());
        }
        CollectTextureUsers(*child, texture, out);
      }
    }

    /// <summary>
    /// 背景付きの文字を描く（画像の上でも読めるように）
    /// </summary>
    void AddLabel(ImDrawList* drawList, const ImVec2& pos, const std::string& text) {
      const ImVec2 size = ImGui::CalcTextSize(text.c_str());
      drawList->AddRectFilled(ImVec2(pos.x - 2.0f, pos.y - 1.0f), ImVec2(pos.x + size.x + 2.0f, pos.y + size.y + 1.0f), kLabelBackColor);
      drawList->AddText(pos, IM_COL32_WHITE, text.c_str());
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
    if (!discardLayout_.empty()) {
      if (UILayout* gameLayout = FindGameLayout(discardLayout_)) {
        gameLayout->Load(discardLayout_);
      }
      discardLayout_.clear();
    }

    SyncPreview();

    // シーンの切り替えなどでインスタンスごと消えた編集は戻せないので知らせる
    const std::vector<UILayout*>& instances = UILayout::GetInstances();
    if (editedLayout_ && std::ranges::find(instances, editedLayout_) == instances.end()) {
      AddLog(std::format("UI Editor: unsaved changes to '{}' were lost because the layout was unloaded", layoutName_), DebugUIManager::LogType::Warning);
      editedLayout_ = nullptr;
    }

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

    // UI を描く区間の前後で保存内容を比べて編集を検出する（この区間ではゲームのコードが値を変えない）
    UILayout* const   editing         = FindLayout();
    const std::string editingSnapshot = editing ? editing->Serialize() : std::string();
    const std::string sliceSnapshot   = UILayout::SerializeTextureSlices();

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
    const bool isFocused = isEditorFocused || (isViewportFocused_ && isViewportEditEnabled_);
    wantsKeyboard_ = selected && !ImGui::GetIO().WantTextInput && isFocused;
    if (wantsKeyboard_) {
      HandleArrowKeys(*selected);
    }
    isViewportFocused_ = false;

    // 途中で別のレイアウトへ切り替えた場合は比べない
    if (editing && editing == layout) {
      TrackLayoutEdit(*editing, editingSnapshot);
    }
    if (const std::string slices = UILayout::SerializeTextureSlices(); slices != sliceSnapshot) {
      isSliceTableEdited_ = IsStillEdited(isSliceTableEdited_, sliceSnapshot, slices, cleanSliceSnapshot_);
    }

    // 比べた後に保存して、このフレームの編集も保存済みにする。ビューポートのフォーカスは ImGui のルーティングで届かないので自前で判定する
    if (layout && isFocused && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
      SaveLayout(*layout);
    }
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
    const std::string     snapshot = layout->Serialize();

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

    TrackLayoutEdit(*layout, snapshot);
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

    if (preview_ && (!isPreviewMode_ || preview_->GetName() != layoutName_)) {
      preview_.reset();
    }
    if (isPreviewMode_ && !preview_) {
      preview_ = std::make_unique<UILayout>();
      preview_->Load(layoutName_);
    }
  }

  void UILayoutEditor::RequestOpen(const OpenRequest& request) {
    if (editedLayout_) {
      pendingOpen_             = request;
      isUnsavedPopupRequested_ = true;
      return;
    }
    Open(request);
  }

  void UILayoutEditor::Open(const OpenRequest& request) {
    editedLayout_ = nullptr;
    if (request.isNew) {
      // 古いプレビューの破棄（スプライト解放）を伴うので、作成は次の Update で行う
      pendingNewLayout_ = request.name;
      return;
    }
    layoutName_    = request.name;
    isPreviewMode_ = request.isPreview;
    Select(nullptr);
    selectedName_ = request.selectName;
  }

  bool UILayoutEditor::SaveLayout(const UILayout& layout) {
    const std::string path = LayoutFilePath(layout.GetName());
    if (!layout.Save()) {
      AddLog("UILayout save failed: " + path, DebugUIManager::LogType::Error);
      return false;
    }
    AddLog("UILayout saved: " + path);
    if (editedLayout_ == &layout) {
      editedLayout_ = nullptr;
    }

    // 共有の 9-slice はレイアウトをまたぐが、どのレイアウトの Save でも一緒に保存する
    if (isSliceTableEdited_) {
      if (!UILayout::SaveTextureSlices()) {
        AddLog(std::string("Texture 9-slice save failed: ") + UILayout::kTextureSlicePath, DebugUIManager::LogType::Error);
        return false;
      }
      AddLog(std::string("Texture 9-slice saved: ") + UILayout::kTextureSlicePath);
      isSliceTableEdited_ = false;
    }
    return true;
  }

  void UILayoutEditor::CollectUnsavedChanges(std::vector<std::string>& out) const {
    if (editedLayout_) {
      out.push_back("UI Editor: " + layoutName_);
    }
    if (isSliceTableEdited_) {
      out.push_back(std::string("UI Editor: texture 9-slice settings (") + UILayout::kTextureSlicePath + ")");
    }
  }

  UIElement* UILayoutEditor::FindSelected(UILayout& layout) const {
    return selectedName_.empty() ? nullptr : layout.Find(selectedName_);
  }

  void UILayoutEditor::Select(const UIElement* element) {
    selectedName_     = element ? element->name : std::string();
    isRenaming_       = false;
    isRenameRejected_ = false;
    isPreviewFit_     = true;
  }

  void UILayoutEditor::DrawToolbar(UILayout* layout) {
    auto open = [this](const std::string& name, bool isPreview) {
      if (name != layoutName_ || isPreview != isPreviewMode_) {
        RequestOpen({ .name = name, .isPreview = isPreview });
      }
    };

    std::string comboLabel = layoutName_.empty() ? "(none)" : (isPreviewMode_ ? layoutName_ + " (preview)" : layoutName_);
    if (layout && layout == editedLayout_) {
      comboLabel += " *";
    }
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

    std::function<bool()> save;
    if (layout) {
      save = [this, layout] { return SaveLayout(*layout); };
    }
    const std::string     message = std::format("Layout '{}' has unsaved changes.", layoutName_);
    const UnsavedChoice   choice  = DrawUnsavedChangesModal("Unsaved Changes##UIEditor", isUnsavedPopupRequested_, message, "Switch", save);
    if (choice == UnsavedChoice::Proceed && pendingOpen_) {
      // ゲーム側のレイアウトはメモリ上の編集が残るので、ファイルから読み直して捨てる（保存した場合は同じ内容に戻るだけ）
      if (layout && layout != preview_.get()) {
        discardLayout_ = layout->GetName();
      }
      Open(*pendingOpen_);
    }
    if (choice != UnsavedChoice::None) {
      pendingOpen_.reset();
    }

    ImGui::BeginDisabled(!layout);
    ImGui::SameLine();
    // 共有の 9-slice の変更はレイアウト名の * に出ないので、ボタンにも付ける
    const bool isUnsaved = (layout && layout == editedLayout_) || isSliceTableEdited_;
    if (ImGui::Button(isUnsaved ? "Save *###Save" : "Save###Save")) {
      SaveLayout(*layout);
    }
    ImGui::SetItemTooltip("Save the layout and the shared texture 9-slice settings (Ctrl+S).");
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      Request(PendingOp::Reload, {});
    }
    ImGui::SetItemTooltip("Discard unsaved changes to the layout and the shared texture 9-slice settings.");
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
        RequestOpen({ .name = name, .isPreview = true, .isNew = true });
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
    const char* const kind   = element.texture.empty() ? "  [Group]" : (element.isSliced ? "  [9-Slice]" : "");
    const bool        isOpen = ImGui::TreeNodeEx("##Node", flags, "%s%s", element.name.c_str(), kind);
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
    if (element.sprite) {
      ImGui::TextUnformatted("Image Type");
      ImGui::SameLine();
      if (ImGui::RadioButton("Simple", !element.isSliced)) {
        element.isSliced = false;
      }
      ImGui::SetItemTooltip("Stretch the whole UV rect to the element size.");
      ImGui::SameLine();
      if (ImGui::RadioButton("9-Slice", element.isSliced) && !element.isSliced) {
        element.isSliced = true;
        isSliceDragMode_ = true;
      }
      ImGui::SetItemTooltip("Keep the corners at their size and stretch only the edges and the center.");
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
    }

    // 9-slice はコピーを編集し、変わったときだけ書き戻す（共有か要素の上書きかで書き先が違う）
    UISlice    slice;
    UISlice    before;
    const bool isSliced = element.sprite && element.isSliced;
    if (isSliced) {
      ImGui::SeparatorText("9-Slice");
      ImGui::TextUnformatted("Source");
      ImGui::SameLine();
      if (ImGui::RadioButton("Texture (shared)", !element.isSliceOverridden)) {
        element.isSliceOverridden = false;
      }
      ImGui::SetItemTooltip("Use the 9-slice saved for this texture. It is shared by every element using the texture, in all layouts.");
      ImGui::SameLine();
      if (ImGui::RadioButton("This element", element.isSliceOverridden) && !element.isSliceOverridden) {
        // 共有の値から上書きを始める
        element.slice             = UILayout::GetTextureSlice(element.texture);
        element.isSliceOverridden = true;
      }
      ImGui::SetItemTooltip("Override the shared 9-slice for this element only.");

      slice  = UILayout::GetEffectiveSlice(element);
      before = slice;
      DrawSliceSettings(layout, element, slice);
    }

    ImGui::PopID();

    // プレビューの高さは要素をまたいで共有するので、要素ごとの ID の外で描く
    if (element.sprite) {
      DrawTexturePreview(element, slice);
    }
    if (isSliced) {
      if (isSliceMirrored_) {
        MirrorBorder(slice.border, before.border, element.uvSize);
      }
      DrawSliceResult(layout, element, slice);

      if (slice != before) {
        if (element.isSliceOverridden) {
          element.slice = slice;
        }
        else {
          UILayout::SetTextureSlice(element.texture, slice);
        }
      }
    }
  }

  void UILayoutEditor::DrawSliceUsers(const UIElement& element) {
    // 要素名はレイアウトの中でしか一意でないので、レイアウトと組で持つ
    std::vector<std::pair<UILayout*, UIElement*>> users;
    for (UILayout* instance : UILayout::GetInstances()) {
      if (instance->GetName().empty()) {
        continue;
      }
      std::vector<UIElement*> found;
      CollectTextureUsers(instance->GetRoot(), element.texture, found);
      for (UIElement* user : found) {
        users.emplace_back(instance, user);
      }
    }
    const auto sharedCount = std::ranges::count_if(users, [](const auto& user) { return !user.second->isSliceOverridden; });

    const std::string label = std::format("Shared by {} element(s)...###SliceUsers", sharedCount);
    if (ImGui::SmallButton(label.c_str())) {
      ImGui::OpenPopup("SliceUsers");
    }
    ImGui::SetItemTooltip("List the elements in open layouts that use %s, and jump to one.", element.texture.c_str());
    if (!ImGui::BeginPopup("SliceUsers")) {
      return;
    }

    UILayout* const current = FindLayout();
    for (const auto& [instance, user] : users) {
      const std::string row = std::format("{} / {}{}", instance->GetName(), user->name, user->isSliceOverridden ? "  (override)" : "");
      ImGui::PushID(user);
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(user->isSliceOverridden ? ImGuiCol_TextDisabled : ImGuiCol_Text));
      const bool isClicked = ImGui::Selectable(row.c_str(), user == &element);
      ImGui::PopStyleColor();
      ImGui::PopID();
      if (!isClicked) {
        continue;
      }
      // 別のレイアウトは、未保存の確認を通して開いてから選ぶ
      if (instance == current) {
        Select(user);
      }
      else {
        RequestOpen({ .name = instance->GetName(), .selectName = user->name });
      }
    }
    ImGui::EndPopup();
  }

  void UILayoutEditor::DrawSliceSettings(const UILayout& layout, UIElement& element, UISlice& slice) {
    // 説明と警告は長いので、インスペクターの幅で折り返す
    ImGui::PushTextWrapPos(0.0f);
    if (element.isSliceOverridden) {
      if (ImGui::Button("Save as Texture Default")) {
        UILayout::SetTextureSlice(element.texture, slice);
      }
      ImGui::SetItemTooltip("Copy this element's 9-slice to the shared 9-slice of %s.", element.texture.c_str());
      DrawSliceUsers(element);
    }
    else {
      DrawSliceUsers(element);
      ImGui::TextDisabled("Edits apply to all of them (in every layout).");
    }

    ImGui::DragFloat4("Border", &slice.border.x, 1.0f, 0.0f, 4096.0f, "%.0f");
    ImGui::SetItemTooltip("Left, Top, Right, Bottom in texture px, measured from each edge of the UV rect.\nCorners keep this size on screen; edges stretch along one axis and the center stretches along both.");
    float all = slice.border.x;
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * 0.5f);
    if (ImGui::DragFloat("All", &all, 1.0f, 0.0f, 4096.0f, "%.0f")) {
      slice.border = { all, all, all, all };
    }
    ImGui::SameLine();
    ImGui::Checkbox("Mirror", &isSliceMirrored_);
    ImGui::SetItemTooltip("Keep Left = Right and Top = Bottom while editing.");

    // 枠の合計が要素より大きいと、四隅が比率のまま縮み中央は描かれない（Sprite::CalcSliceGrid）
    const float scale = layout.GetScale();
    const float sumX  = slice.border.x + slice.border.z;
    const float sumY  = slice.border.y + slice.border.w;
    if (sumX * scale > element.rectSize.x) {
      ImGui::TextColored(kWarningColor, "L + R (%.0f px on screen) is wider than the element (%.0f px). Corners are shrunk to fit and the center is not drawn.", sumX * scale, element.rectSize.x);
    }
    if (sumY * scale > element.rectSize.y) {
      ImGui::TextColored(kWarningColor, "T + B (%.0f px on screen) is taller than the element (%.0f px). Corners are shrunk to fit and the center is not drawn.", sumY * scale, element.rectSize.y);
    }
    if (sumX > element.uvSize.x || sumY > element.uvSize.y) {
      ImGui::TextColored(kWarningColor, "The border is larger than the UV rect (%.0f x %.0f), so the center region is flipped.", element.uvSize.x, element.uvSize.y);
    }
    ImGui::PopTextWrapPos();

    // 区画は 3x3 のボタンで選ぶ（プレビューの 9-Slice モードのクリックでも選べる）
    ImGui::TextUnformatted("Regions");
    ImGui::SetItemTooltip("Dim: hidden. *: uses its own image.");
    const float buttonSize = ImGui::GetFrameHeight() * 1.4f;
    for (int region = 0; region < UISlice::kRegionCount; ++region) {
      if (region % 3 != 0) {
        ImGui::SameLine();
      }
      const bool        isSelected = region == selectedRegion_;
      const std::string label      = std::format("{}{}##Region{}", kRegionLabels[region], slice.regionTextures[region].empty() ? "" : "*", region);
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(isSelected ? ImGuiCol_ButtonActive : ImGuiCol_Button));
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(slice.regionVisible[region] ? ImGuiCol_Text : ImGuiCol_TextDisabled));
      if (ImGui::Button(label.c_str(), ImVec2(buttonSize, buttonSize))) {
        selectedRegion_ = region;
      }
      ImGui::PopStyleColor(2);
      ImGui::SetItemTooltip("%s", kRegionNames[region]);
    }

    const auto [regionMin, regionMax] = RegionRect(element, slice.border, selectedRegion_);
    ImGui::Text("%s: %.0f x %.0f px at (%.0f, %.0f)", kRegionNames[selectedRegion_], regionMax.x - regionMin.x, regionMax.y - regionMin.y, regionMin.x, regionMin.y);
    ImGui::Checkbox("Visible##Region", &slice.regionVisible[selectedRegion_]);
    ImGui::SetItemTooltip(selectedRegion_ == kCenterRegion ? "Unchecked: draw only the border (Fill Center off)." : "Unchecked: this region is not drawn.");

    // 区画ごとの別画像。区画の大きさは枠で決まり、画像はその区画いっぱいに引き伸ばす
    std::string& regionTexture = slice.regionTextures[selectedRegion_];
    if (ImGui::BeginCombo("Region Texture", regionTexture.empty() ? "(element texture)" : regionTexture.c_str())) {
      if (ImGui::Selectable("(element texture)", regionTexture.empty())) {
        regionTexture.clear();
      }
      for (const std::string& name : TextureManager::GetInstance()->GetLoadedTextureFileNames()) {
        if (ImGui::Selectable(name.c_str(), name == regionTexture)) {
          regionTexture = name;
        }
      }
      ImGui::EndCombo();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowser::kTexturePayload)) {
        regionTexture = static_cast<const char*>(payload->Data);
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::SetItemTooltip("Use another image for this region. It is stretched to fill the region; the border decides the region size.");

    TextureManager* textureManager = TextureManager::GetInstance();
    const bool      isOwnLoaded    = !regionTexture.empty() && textureManager->IsLoaded(regionTexture);
    ImGui::BeginDisabled(regionTexture.empty());
    if (ImGui::Button("Clear##RegionTexture")) {
      regionTexture.clear();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // 中央には合わせる枠が無い
    ImGui::BeginDisabled(!isOwnLoaded || selectedRegion_ == kCenterRegion);
    if (ImGui::Button("Use Image Size as Border")) {
      const DirectX::TexMetadata& metadata = textureManager->GetMetaData(regionTexture);
      const float                 width    = static_cast<float>(metadata.width);
      const float                 height   = static_cast<float>(metadata.height);
      const int                   col      = selectedRegion_ % 3;
      const int                   row      = selectedRegion_ / 3;
      if (col == 0) {
        slice.border.x = width;
      }
      if (col == 2) {
        slice.border.z = width;
      }
      if (row == 0) {
        slice.border.y = height;
      }
      if (row == 2) {
        slice.border.w = height;
      }
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Set the border sides touching this region to the image size (corners set two sides, edges set one).");
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

  void UILayoutEditor::DrawTexturePreview(UIElement& element, UISlice& slice) {
    constexpr float kMinZoom    = 0.01f;
    constexpr float kMaxZoom    = 64.0f;
    constexpr float kZoomStep   = 1.2f;   ///< ホイール 1 段あたりの倍率
    constexpr float kFitMargin  = 12.0f;  ///< 全体表示で四辺に空ける余白（px）
    constexpr float kTextMargin = 4.0f;

    TextureManager* textureManager = TextureManager::GetInstance();
    if (!textureManager->IsLoaded(element.texture)) {
      return;
    }
    // ImGui のシェーダーは Texture2D 前提
    const DirectX::TexMetadata& metadata = textureManager->GetMetaData(element.texture);
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.IsCubemap()) {
      return;
    }
    const Vector2 texSize = { static_cast<float>(metadata.width), static_cast<float>(metadata.height) };

    ImGui::SeparatorText("Preview");
    if (element.isSliced) {
      if (ImGui::RadioButton("9-Slice", isSliceDragMode_)) {
        isSliceDragMode_ = true;
      }
      ImGui::SetItemTooltip("Left drag moves the 9-slice lines (green). Click inside a region to select it.");
      ImGui::SameLine();
      if (ImGui::RadioButton("UV", !isSliceDragMode_)) {
        isSliceDragMode_ = false;
      }
      ImGui::SetItemTooltip("Left drag moves or resizes the UV rect (yellow).");
      ImGui::SameLine();
    }
    // Simple の要素では UV の枠だけを動かす
    const bool isSliceDrag = element.isSliced && isSliceDragMode_;
    if (ImGui::Button("Fit")) {
      isPreviewFit_ = true;
    }
    ImGui::SetItemTooltip("Show the whole texture and UV rect. Wheel: zoom, Middle drag: pan, Bottom edge: resize.");

    if (!ImGui::BeginChild("TexturePreview##UIEditor", ImVec2(0.0f, kPreviewDefaultHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
      ImGui::EndChild();
      return;
    }

    const ImGuiIO& io        = ImGui::GetIO();
    const ImVec2   canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2   avail     = ImGui::GetContentRegionAvail();
    const ImVec2   canvasSize((std::max)(avail.x, 1.0f), (std::max)(avail.y, 1.0f));
    const ImVec2   canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImGui::InvisibleButton("##Canvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    // ホイールを拡大縮小に使う。NoScrollWithMouse の子ウィンドウはホイールを親へ回すので、それだけではインスペクターがスクロールする
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive  = ImGui::IsItemActive();
    if (!isActive) {
      previewDragEdges_ = 0;
    }

    const Vector2 uvMin = element.uvLeftTop;
    const Vector2 uvMax = element.uvLeftTop + element.uvSize;

    // ドラッグ中に倍率が変わると、動かした線が全体表示を広げてさらに線が動く、を繰り返すので合わせ直さない
    if (isPreviewFit_ && previewDragEdges_ == 0) {
      const Vector2 boundsMin = { (std::min)(0.0f, uvMin.x), (std::min)(0.0f, uvMin.y) };
      const Vector2 boundsMax = { (std::max)(texSize.x, uvMax.x), (std::max)(texSize.y, uvMax.y) };
      const Vector2 bounds    = boundsMax - boundsMin;
      const float   fitZoom   = (std::min)((canvasSize.x - kFitMargin * 2.0f) / bounds.x, (canvasSize.y - kFitMargin * 2.0f) / bounds.y);
      previewZoom_   = std::clamp(fitZoom, kMinZoom, kMaxZoom);
      previewOrigin_ = Vector2{ canvasSize.x, canvasSize.y } * 0.5f - (boundsMin + bounds * 0.5f) * previewZoom_;
    }

    const Vector2 mouseLocal = { io.MousePos.x - canvasMin.x, io.MousePos.y - canvasMin.y };
    if (isHovered && io.MouseWheel != 0.0f) {
      // マウスの下にあるテクスチャ上の点を動かさずに拡大縮小する
      const float zoom = std::clamp(previewZoom_ * std::pow(kZoomStep, io.MouseWheel), kMinZoom, kMaxZoom);
      previewOrigin_ = mouseLocal - (mouseLocal - previewOrigin_) * (zoom / previewZoom_);
      previewZoom_   = zoom;
      isPreviewFit_  = false;
    }
    if (isActive && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      previewOrigin_ += Vector2{ io.MouseDelta.x, io.MouseDelta.y };
      isPreviewFit_   = false;
    }

    auto          toScreen = [&](float x, float y) { return ImVec2(canvasMin.x + previewOrigin_.x + x * previewZoom_, canvasMin.y + previewOrigin_.y + y * previewZoom_); };
    const Vector2 mouseTex = (mouseLocal - previewOrigin_) / previewZoom_;

    // 動かす線（テクスチャ px、左上右下）
    const Vector4& border = slice.border;
    const Vector4  lines  = isSliceDrag
      ? Vector4{ uvMin.x + border.x, uvMin.y + border.y, uvMax.x - border.z, uvMax.y - border.w }
      : Vector4{ uvMin.x, uvMin.y, uvMax.x, uvMax.y };

    int hoveredEdges = previewDragEdges_;
    if (hoveredEdges == 0 && isHovered) {
      const ImVec2 spanMin  = toScreen(uvMin.x, uvMin.y);
      const ImVec2 spanMax  = toScreen(uvMax.x, uvMax.y);
      const ImVec2 linesMin = toScreen(lines.x, lines.y);
      const ImVec2 linesMax = toScreen(lines.z, lines.w);
      hoveredEdges = HitEdges(io.MousePos, { linesMin.x, linesMin.y, linesMax.x, linesMax.y }, spanMin, spanMax, kHandleHalfSize + kHitMargin);
      // UV の枠の中は枠ごと動かす
      if (hoveredEdges == 0 && !isSliceDrag &&
        io.MousePos.x >= spanMin.x && io.MousePos.x <= spanMax.x && io.MousePos.y >= spanMin.y && io.MousePos.y <= spanMax.y) {
        hoveredEdges = kEdgeAll;
      }
    }
    if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      if (hoveredEdges != 0) {
        previewDragEdges_      = hoveredEdges;
        previewDragStartMouse_ = mouseTex;
        previewDragStartLines_ = lines;
      }
      else if (isSliceDrag && mouseTex.x >= uvMin.x && mouseTex.x <= uvMax.x && mouseTex.y >= uvMin.y && mouseTex.y <= uvMax.y) {
        // 線以外のクリックは、その位置の区画を選ぶ
        const int col   = mouseTex.x < lines.x ? 0 : (mouseTex.x < lines.z ? 1 : 2);
        const int row   = mouseTex.y < lines.y ? 0 : (mouseTex.y < lines.w ? 1 : 2);
        selectedRegion_ = row * 3 + col;
      }
    }
    if (hoveredEdges != 0) {
      ImGui::SetMouseCursor(EdgeCursor(hoveredEdges));
    }

    // ドラッグ量をテクスチャ px の整数に丸め、掴んだ線だけに足す
    if (previewDragEdges_ != 0) {
      const int   edges  = previewDragEdges_;
      const float deltaX = std::round(mouseTex.x - previewDragStartMouse_.x);
      const float deltaY = std::round(mouseTex.y - previewDragStartMouse_.y);
      Vector4     moved  = previewDragStartLines_;
      if (edges & kEdgeLeft) {
        moved.x += deltaX;
      }
      if (edges & kEdgeTop) {
        moved.y += deltaY;
      }
      if (edges & kEdgeRight) {
        moved.z += deltaX;
      }
      if (edges & kEdgeBottom) {
        moved.w += deltaY;
      }

      if (isSliceDrag) {
        // 反対側の線を越えない
        Vector4& edited = slice.border;
        if (edges & kEdgeLeft) {
          edited.x = std::clamp(moved.x - uvMin.x, 0.0f, (std::max)(element.uvSize.x - edited.z, 0.0f));
        }
        if (edges & kEdgeTop) {
          edited.y = std::clamp(moved.y - uvMin.y, 0.0f, (std::max)(element.uvSize.y - edited.w, 0.0f));
        }
        if (edges & kEdgeRight) {
          edited.z = std::clamp(uvMax.x - moved.z, 0.0f, (std::max)(element.uvSize.x - edited.x, 0.0f));
        }
        if (edges & kEdgeBottom) {
          edited.w = std::clamp(uvMax.y - moved.w, 0.0f, (std::max)(element.uvSize.y - edited.y, 0.0f));
        }
      }
      else {
        // 左上は 0 以上（UV Left Top の入力欄と同じ範囲）。移動では大きさを保ったまま止め、辺では 1 px 以上残す
        if (edges == kEdgeAll) {
          const float shiftX = (std::max)(-moved.x, 0.0f);
          const float shiftY = (std::max)(-moved.y, 0.0f);
          moved = { moved.x + shiftX, moved.y + shiftY, moved.z + shiftX, moved.w + shiftY };
        }
        else {
          if (edges & kEdgeLeft) {
            moved.x = std::clamp(moved.x, 0.0f, (std::max)(moved.z - 1.0f, 0.0f));
          }
          if (edges & kEdgeTop) {
            moved.y = std::clamp(moved.y, 0.0f, (std::max)(moved.w - 1.0f, 0.0f));
          }
          if (edges & kEdgeRight) {
            moved.z = (std::max)(moved.z, moved.x + 1.0f);
          }
          if (edges & kEdgeBottom) {
            moved.w = (std::max)(moved.w, moved.y + 1.0f);
          }
        }
        element.uvLeftTop = { moved.x, moved.y };
        element.uvSize    = { moved.z - moved.x, moved.w - moved.y };
      }
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvasMin, canvasMax, true);

    const ImVec2 texMin = toScreen(0.0f, 0.0f);
    const ImVec2 texMax = toScreen(texSize.x, texSize.y);
    drawList->AddImage(static_cast<ImTextureID>(textureManager->GetSRVGPUHandle(element.texture).ptr), texMin, texMax);
    drawList->AddRect(texMin, texMax, kTextureColor);

    // 編集後の値で描く
    const ImVec2   rectMin = toScreen(element.uvLeftTop.x, element.uvLeftTop.y);
    const ImVec2   rectMax = toScreen(element.uvLeftTop.x + element.uvSize.x, element.uvLeftTop.y + element.uvSize.y);
    const Vector4& edited  = slice.border;
    if (isSliceDrag) {
      const auto [regionMin, regionMax] = RegionRect(element, edited, selectedRegion_);
      drawList->AddRectFilled(toScreen(regionMin.x, regionMin.y), toScreen(regionMax.x, regionMax.y), kRegionFillColor);
    }
    drawList->AddRect(rectMin, rectMax, kUvColor, 0.0f, 0, isSliceDrag ? 1.0f : 2.0f);
    if (!isSliceDrag) {
      for (const ImVec2& corner : { rectMin, ImVec2(rectMax.x, rectMin.y), rectMax, ImVec2(rectMin.x, rectMax.y) }) {
        drawList->AddRectFilled(ImVec2(corner.x - kHandleHalfSize, corner.y - kHandleHalfSize), ImVec2(corner.x + kHandleHalfSize, corner.y + kHandleHalfSize), kUvColor);
      }
    }

    // 9-slice モードでは、枠が 0 の線も UV の辺の上に描いて掴めることを示す。掴める線は太くする
    auto drawSliceLine = [&](int edge, float width, const ImVec2& from, const ImVec2& to) {
      if (!isSliceDrag && width <= 0.0f) {
        return;
      }
      drawList->AddLine(from, to, kSliceColor, (isSliceDrag && (hoveredEdges & edge)) ? 3.0f : 1.0f);
    };
    const float sliceLeft   = rectMin.x + edited.x * previewZoom_;
    const float sliceTop    = rectMin.y + edited.y * previewZoom_;
    const float sliceRight  = rectMax.x - edited.z * previewZoom_;
    const float sliceBottom = rectMax.y - edited.w * previewZoom_;
    drawSliceLine(kEdgeLeft, edited.x, ImVec2(sliceLeft, rectMin.y), ImVec2(sliceLeft, rectMax.y));
    drawSliceLine(kEdgeTop, edited.y, ImVec2(rectMin.x, sliceTop), ImVec2(rectMax.x, sliceTop));
    drawSliceLine(kEdgeRight, edited.z, ImVec2(sliceRight, rectMin.y), ImVec2(sliceRight, rectMax.y));
    drawSliceLine(kEdgeBottom, edited.w, ImVec2(rectMin.x, sliceBottom), ImVec2(rectMax.x, sliceBottom));

    if (isHovered) {
      const std::string text = std::format("{}, {} px", static_cast<int>(std::floor(mouseTex.x)), static_cast<int>(std::floor(mouseTex.y)));
      AddLabel(drawList, ImVec2(canvasMin.x + kTextMargin, canvasMax.y - ImGui::GetTextLineHeight() - kTextMargin), text);
    }

    drawList->PopClipRect();
    ImGui::EndChild();
  }

  void UILayoutEditor::DrawSliceResult(const UILayout& layout, const UIElement& element, const UISlice& slice) {
    constexpr float kFitMargin  = 8.0f;  ///< 四辺に空ける余白（px）
    constexpr float kTextMargin = 4.0f;

    TextureManager* textureManager = TextureManager::GetInstance();
    const Vector2   size           = element.rectSize;
    if (!textureManager->IsLoaded(element.texture) || size.x <= 0.0f || size.y <= 0.0f) {
      return;
    }
    const DirectX::TexMetadata& metadata = textureManager->GetMetaData(element.texture);
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.IsCubemap()) {
      return;
    }

    ImGui::SeparatorText("Result");
    if (!ImGui::BeginChild("SliceResult##UIEditor", ImVec2(0.0f, kResultDefaultHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
      ImGui::EndChild();
      return;
    }

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 avail     = ImGui::GetContentRegionAvail();
    const float  zoom      = (std::min)((avail.x - kFitMargin * 2.0f) / size.x, (avail.y - kFitMargin * 2.0f) / size.y);
    if (zoom <= 0.0f) {
      ImGui::EndChild();
      return;
    }

    // 要素の矩形を中央に置き、単位空間（0..1）の格子をそこへ写す
    const ImVec2 origin(canvasMin.x + (avail.x - size.x * zoom) * 0.5f, canvasMin.y + (avail.y - size.y * zoom) * 0.5f);
    auto toScreen = [&](float unitX, float unitY) { return ImVec2(origin.x + unitX * size.x * zoom, origin.y + unitY * size.y * zoom); };

    const Vector2           texSize  = { static_cast<float>(metadata.width), static_cast<float>(metadata.height) };
    const Sprite::SliceGrid grid     = Sprite::CalcSliceGrid(size, slice.border, layout.GetScale(), element.uvLeftTop, element.uvSize, texSize);
    const ImU32             tint     = ImGui::ColorConvertFloat4ToU32(ImVec4(element.color.x, element.color.y, element.color.z, element.color.w));
    ImDrawList*             drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(canvasMin, ImVec2(canvasMin.x + avail.x, canvasMin.y + avail.y), true);

    for (int region = 0; region < UISlice::kRegionCount; ++region) {
      if (!slice.regionVisible[region]) {
        continue;
      }
      const int          col     = region % 3;
      const int          row     = region / 3;
      const std::string& own     = slice.regionTextures[region];
      const bool         isOwn   = !own.empty() && textureManager->IsLoaded(own);
      const std::string& texture = isOwn ? own : element.texture;
      const ImVec2       uvMin   = isOwn ? ImVec2(0.0f, 0.0f) : ImVec2(grid.us[col], grid.vs[row]);
      const ImVec2       uvMax   = isOwn ? ImVec2(1.0f, 1.0f) : ImVec2(grid.us[col + 1], grid.vs[row + 1]);
      drawList->AddImage(static_cast<ImTextureID>(textureManager->GetSRVGPUHandle(texture).ptr),
        toScreen(grid.xs[col], grid.ys[row]), toScreen(grid.xs[col + 1], grid.ys[row + 1]), uvMin, uvMax, tint);
    }

    // 区画の境目と要素の外枠
    for (int i = 1; i <= 2; ++i) {
      drawList->AddLine(toScreen(grid.xs[i], 0.0f), toScreen(grid.xs[i], 1.0f), kResultLineColor);
      drawList->AddLine(toScreen(0.0f, grid.ys[i]), toScreen(1.0f, grid.ys[i]), kResultLineColor);
    }
    drawList->AddRect(toScreen(0.0f, 0.0f), toScreen(1.0f, 1.0f), kTextureColor);

    AddLabel(drawList, ImVec2(canvasMin.x + kTextMargin, canvasMin.y + kTextMargin), std::format("{:.0f} x {:.0f} px (x{:.2f})", size.x, size.y, zoom));

    drawList->PopClipRect();
    ImGui::EndChild();
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
    const bool        isRoot   = target == &layout.GetRoot();
    const std::string snapshot = layout.Serialize();

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
        isPreviewFit_ = true;
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
      UILayout::LoadTextureSlices();
      layout.Load(layout.GetName());
      dragMode_           = DragMode::None;
      editedLayout_       = nullptr;
      isSliceTableEdited_ = false;
      AddLog("UILayout reloaded: " + layout.GetName());
      return;
    case PendingOp::None:
      break;
    }

    TrackLayoutEdit(layout, snapshot);
  }

  void UILayoutEditor::TrackLayoutEdit(const UILayout& layout, const std::string& before) {
    const std::string after = layout.Serialize();
    if (after != before) {
      editedLayout_ = IsStillEdited(editedLayout_ == &layout, before, after, cleanSnapshot_) ? &layout : nullptr;
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
