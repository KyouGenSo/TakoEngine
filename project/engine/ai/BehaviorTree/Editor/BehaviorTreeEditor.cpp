#ifdef _DEBUG

#include "BehaviorTreeEditor.h"
#include "BTChildSort.h"
#include "BTComposite.h"
#include "BTBlackboard.h"
#include "BTNodeRegistry.h"
#include "DebugUIManager.h"
#include "Hash.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <format>
#include <filesystem>

namespace Tako {

namespace {

std::vector<BehaviorTreeEditor*>& Instances() {
  static std::vector<BehaviorTreeEditor*> instances;
  return instances;
}

} // namespace

BehaviorTreeEditor::BehaviorTreeEditor() {
  Instances().push_back(this);
}

BehaviorTreeEditor::~BehaviorTreeEditor() {
  std::erase(Instances(), this);
}

const std::vector<BehaviorTreeEditor*>& BehaviorTreeEditor::GetInstances() {
  return Instances();
}

void BehaviorTreeEditor::CollectUnsavedChanges(std::vector<std::string>& out) const {
  if (treeTracker_.IsEdited()) {
    out.push_back(std::format("{}: {}", config_.windowName, currentTreeName_));
  }
}

void BehaviorTreeEditor::Initialize(const EditorConfig& config) {
  config_ = config;

  BTNodeRegistry::GetInstance()->Initialize();

  std::error_code ec;
  std::filesystem::create_directories(config_.btJsonDir, ec);
  if (ec) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Failed to create btJsonDir: " + config_.btJsonDir + " (" + ec.message() + ")",
      DebugUIManager::LogType::Error);
  }

  editorConfig_ = std::make_unique<ed::Config>();
  editorConfig_->NavigateButtonIndex = 1;        // マウス中ボタンでナビゲート
  editorConfig_->ContextMenuButtonIndex = 2;     // マウス右ボタンでコンテキストメニュー
  editorConfig_->SettingsFile = nullptr;

  currentTreeName_ = config_.initialTreeFile.empty()
    ? "default"
    : std::filesystem::path(config_.initialTreeFile).stem().string();

  editorContext_ = ed::CreateEditor(editorConfig_.get());

  if (!config_.initialTreeFile.empty()) {
    LoadFromJSON(config_.btJsonDir + config_.initialTreeFile);
  }
}

void BehaviorTreeEditor::Finalize() {
  if (editorContext_) {
    ed::SetCurrentEditor(editorContext_);
    ed::DestroyEditor(editorContext_);
    editorContext_ = nullptr;
  }
  editorConfig_.reset();
  Clear();
}

void BehaviorTreeEditor::Update() {
  if (!isVisible_) return;

  if (ImGui::Begin(config_.windowName.c_str(), &isVisible_)) {
    // UI を描く区間の前後でツリーの内容を比べて編集を検出する（Node Inspector は別のウィンドウなので、別の区間で比べる）
    treeTracker_.BeginSection();
    DrawToolbar();
    ImGui::Separator();

    ed::SetCurrentEditor(editorContext_);
    ed::Begin(config_.canvasName.c_str());

    DrawNodes();
    DrawLinks();

    // Add Node で予約された新規ノード位置を 1 回だけ適用 (ed::Begin の外では効かないため)
    if (!pendingNodePositions_.empty()) {
      for (const auto& [id, pos] : pendingNodePositions_) {
        ed::SetNodePosition(id, pos);
        // ed は位置を整数に丸めるので、移動の検出に使う位置を合わせておく
        if (EditorNode* node = FindNodeById(id)) {
          node->position = ed::GetNodePosition(id);
        }
      }
      pendingNodePositions_.clear();
    }

    HandleLinkCreation();
    HandleDeletion();

    // 選択ノードの取得は ed::End() 前に実行必須
    {
      int selectedCount = ed::GetSelectedObjectCount();
      if (selectedCount > 0) {
        std::vector<ed::NodeId> selectedNodes(selectedCount);
        int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
        selectedNodeId_ = (nodeCount > 0) ? static_cast<int>(selectedNodes[0].Get()) : -1;
      }
      else {
        selectedNodeId_ = -1;
      }
    }

    if (pendingNavigateToContent_) {
      ed::NavigateToContent(0.0f);
      pendingNavigateToContent_ = false;
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);

    treeTracker_.EndSection();
    DrawNodeInspector();
  }
  ImGui::End();
}

void BehaviorTreeEditor::Clear() {
  nodes_.clear();
  pins_.clear();
  links_.clear();
  runtimeNodeToEditorId_.clear();
  parameterHashes_.clear();
  pendingNodePositions_.clear();

  // ID 範囲はノード 10000 / ピン 20000 / リンク 30000 で分離 (imgui-node-editor の ID 競合回避)
  nextNodeId_ = 10000;
  nextPinId_ = 20000;
  nextLinkId_ = 30000;

  highlightedNodeId_ = -1;
  highlightStartTime_ = 0.0f;
  selectedNodeId_ = -1;
  pendingNavigateToContent_ = true;
}

std::span<const ShortcutBinding<BehaviorTreeEditor>> BehaviorTreeEditor::Shortcuts() {
  static constexpr ShortcutBinding<BehaviorTreeEditor> kShortcuts[] = {
    { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the tree", [](BehaviorTreeEditor& editor) { editor.SaveTree(editor.currentTreeName_); } },
    // 以下は imgui-node-editor の処理（ed::Config のボタン割り当て）
    { ImGuiKey_Delete, ImGuiInputFlags_None, "Delete the selected nodes and links" },
    { ImGuiKey_F,      ImGuiInputFlags_None, "Zoom to the selection (Shift: with margin)" },
    { .description = "Pan",          .keyLabel = "RMB drag" },
    { .description = "Zoom",         .keyLabel = "Wheel" },
    { .description = "Context menu", .keyLabel = "MMB click" },
  };
  return kShortcuts;
}

void BehaviorTreeEditor::DrawToolbar() {
  // ノードの段階的オフセット用 (Add Node を連打した時の重なり回避)
  static float nodeOffsetX = 100.0f;
  static float nodeOffsetY = 100.0f;

  // 選択されたノードタイプを保持
  static int selectedNodeTypeIndex = 0;
  static std::string selectedNodeType = "BTSelector";  // デフォルト

  // === Save / Load / Clear (現在のツリーに対する操作) ===
  if (ImGui::Button("Save##bte_toolbar")) {
    SaveTree(currentTreeName_);
  }
  ProcessShortcuts(Shortcuts(), *this);
  ImGui::SameLine();

  if (ImGui::Button("Load##bte_toolbar")) {
    LoadTree(currentTreeName_);
  }
  ImGui::SameLine();

  if (ImGui::Button("Clear##bte_toolbar")) {
    Clear();
    nodeOffsetX = 100.0f;
    nodeOffsetY = 100.0f;
  }
  ImGui::SameLine();
  treeTracker_.DrawButtons();

  // === マルチツリー: 切替ドロップダウン + 新規/削除 ===
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::Text("Tree:");
  ImGui::SameLine();

  std::vector<std::string> availableTrees = ListAvailableTrees();
  // 未保存マークを付けて currentTreeName を表示
  std::string comboPreview = currentTreeName_;
  if (treeTracker_.IsEdited()) comboPreview += " *";

  ImGui::SetNextItemWidth(160);
  if (ImGui::BeginCombo("##TreeCombo", comboPreview.c_str())) {
    for (const auto& tree : availableTrees) {
      bool isSelected = (tree == currentTreeName_);
      if (ImGui::Selectable(tree.c_str(), isSelected)) {
        SwitchTree(tree);
      }
      if (isSelected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();
  if (ImGui::Button("+ New##bte_tree")) {
    ImGui::OpenPopup("New Tree##popup");
  }
  ImGui::SameLine();
  if (ImGui::Button("- Delete##bte_tree")) {
    ImGui::OpenPopup("Delete Tree##popup");
  }

  // 新規ツリー作成ポップアップ
  static char newTreeNameBuf[128] = "";
  static int  newTreeMode = 0;       // 0 = Empty, 1 = Copy from existing
  static int  copySourceIndex = 0;
  if (ImGui::BeginPopupModal("New Tree##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter new tree name (no extension, no underscore prefix):");
    ImGui::InputText("##NewTreeName", newTreeNameBuf, sizeof(newTreeNameBuf));

    ImGui::RadioButton("Empty##newmode", &newTreeMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Copy from existing##newmode", &newTreeMode, 1);

    if (newTreeMode == 1) {
      if (copySourceIndex >= static_cast<int>(availableTrees.size())) copySourceIndex = 0;
      const char* preview = availableTrees.empty() ? "(no trees)" : availableTrees[copySourceIndex].c_str();
      ImGui::SetNextItemWidth(160);
      if (ImGui::BeginCombo("Source##copysrc", preview)) {
        for (int i = 0; i < static_cast<int>(availableTrees.size()); i++) {
          bool sel = (copySourceIndex == i);
          if (ImGui::Selectable(availableTrees[i].c_str(), sel)) copySourceIndex = i;
          if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Create##new")) {
      const std::string name = newTreeNameBuf;
      bool ok = false;
      if (newTreeMode == 0) {
        ok = !name.empty() && RunOrConfirm("Create", [this, name] { return CreateNewTree(name); });
      }
      else if (!name.empty() && !availableTrees.empty()) {
        const std::string source = availableTrees[copySourceIndex];
        ok = RunOrConfirm("Create", [this, source, name] { return CreateTreeFromCopy(source, name); });
      }
      if (ok) {
        newTreeNameBuf[0] = '\0';
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel##new")) {
      newTreeNameBuf[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // 削除確認ポップアップ
  if (ImGui::BeginPopupModal("Delete Tree##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Delete which tree? (currently editing tree cannot be deleted)");
    ImGui::Separator();
    for (const auto& tree : availableTrees) {
      if (tree == currentTreeName_) continue;  // 編集中ツリーは選択肢から除外
      if (ImGui::Button(("Delete: " + tree).c_str())) {
        DeleteTree(tree);
        ImGui::CloseCurrentPopup();
        break;
      }
    }
    ImGui::Separator();
    if (ImGui::Button("Cancel##del")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // 未保存変更モーダル (RunOrConfirm から発動)。保存に失敗したらモーダルを開いたままにする
  unsavedPrompt_.Draw("Unsaved Changes##modal", [this] { return SaveTree(currentTreeName_); });

  // ノード作成用 UI
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  ImGui::Text("Node Type:");
  ImGui::SameLine();

  // 全ノードタイプを Registry から動的に収集
  static std::vector<std::string> allNodeTypes;
  static std::vector<std::string> allDisplayNames;
  allNodeTypes.clear();
  allDisplayNames.clear();

  auto* registry = BTNodeRegistry::GetInstance();

  // Composite ノード
  auto compositeTypes = registry->GetTypesByCategory(NodeCategory::Composite);
  for (const auto& type : compositeTypes) {
    allNodeTypes.push_back(type);
    const NodeMeta* meta = registry->GetMeta(type);
    std::string displayName = meta ? meta->displayName : type;
    allDisplayNames.push_back("[Composite] " + displayName);
  }

  // Action ノード
  auto actionTypes = registry->GetTypesByCategory(NodeCategory::Action);
  for (const auto& type : actionTypes) {
    allNodeTypes.push_back(type);
    const NodeMeta* meta = registry->GetMeta(type);
    std::string displayName = meta ? meta->displayName : type;
    allDisplayNames.push_back("[Action] " + displayName);
  }

  // Condition ノード
  auto conditionTypes = registry->GetTypesByCategory(NodeCategory::Condition);
  for (const auto& type : conditionTypes) {
    allNodeTypes.push_back(type);
    const NodeMeta* meta = registry->GetMeta(type);
    std::string displayName = meta ? meta->displayName : type;
    allDisplayNames.push_back("[Condition] " + displayName);
  }

  // Decorator ノード (将来の拡張用)
  auto decoratorTypes = registry->GetTypesByCategory(NodeCategory::Decorator);
  for (const auto& type : decoratorTypes) {
    allNodeTypes.push_back(type);
    const NodeMeta* meta = registry->GetMeta(type);
    std::string displayName = meta ? meta->displayName : type;
    allDisplayNames.push_back("[Decorator] " + displayName);
  }

  // ドロップダウンリスト
  if (!allNodeTypes.empty()) {
    if (selectedNodeTypeIndex >= static_cast<int>(allDisplayNames.size())) {
      selectedNodeTypeIndex = 0;
    }

    const char* previewValue = selectedNodeTypeIndex < static_cast<int>(allDisplayNames.size())
      ? allDisplayNames[selectedNodeTypeIndex].c_str()
      : "Select Node Type...";

    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("##NodeTypeCombo", previewValue)) {
      for (int i = 0; i < static_cast<int>(allDisplayNames.size()); i++) {
        bool isSelected = (selectedNodeTypeIndex == i);
        if (ImGui::Selectable(allDisplayNames[i].c_str(), isSelected)) {
          selectedNodeTypeIndex = i;
          selectedNodeType = allNodeTypes[i];
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::SameLine();

    if (ImGui::Button("Add Node##bte_toolbar")) {
      ImVec2 centerPos = ImVec2(nodeOffsetX, nodeOffsetY);
      nodeOffsetX += 30.0f;
      nodeOffsetY += 20.0f;
      if (nodeOffsetX > 800.0f) nodeOffsetX = 100.0f;
      if (nodeOffsetY > 600.0f) nodeOffsetY = 100.0f;
      CreateNode(selectedNodeType, centerPos);
    }
  }
}

void BehaviorTreeEditor::DrawNodes() {
  for (auto& node : nodes_) {
    // 位置の適用待ちのノードは ed にまだ正しい位置が無いので読まない
    const bool isPositionPending = std::ranges::any_of(pendingNodePositions_, [&node](const auto& pending) { return pending.first == node.id; });
    if (!isPositionPending) {
      node.position = ed::GetNodePosition(node.id);  // ドラッグ最新位置を同期
    }
    DrawNode(node);
  }
}

void BehaviorTreeEditor::DrawNode(const EditorNode& node) {
  // ノードの基本色 (少し暗めに)
  ImVec4 nodeColor = ImVec4(
    node.color.x * 0.7f,
    node.color.y * 0.7f,
    node.color.z * 0.7f,
    1.0f
  );

  // 実行中ノードのパルスエフェクト計算
  bool isHighlighted = (node.id == highlightedNodeId_);
  float pulseIntensity = 0.0f;
  float borderWidth = 1.5f;
  ImVec4 borderColor = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);

  if (isHighlighted) {
    float elapsed = static_cast<float>(ImGui::GetTime()) - highlightStartTime_;
    pulseIntensity = (sinf(elapsed * 6.0f) + 1.0f) * 0.5f;  // 0.0 - 1.0 振動

    // ボーダー色を時間経過で変化 (オレンジ - 黄色)
    borderColor = ImVec4(
      1.0f,
      0.6f + pulseIntensity * 0.3f,
      0.2f + pulseIntensity * 0.3f,
      1.0f
    );
    borderWidth = 2.5f + pulseIntensity * 1.5f;

    nodeColor = ImVec4(
      node.color.x * 0.7f + pulseIntensity * 0.1f,
      node.color.y * 0.7f + pulseIntensity * 0.1f,
      node.color.z * 0.7f + pulseIntensity * 0.1f,
      1.0f
    );
  }

  ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(nodeColor));
  ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(borderColor));
  ed::PushStyleVar(ed::StyleVar_NodeRounding, 5.0f);
  ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, borderWidth);

  ed::BeginNode(node.id);
  ImGui::PushID(node.id);

  const float nodeWidth = 200.0f;
  const float barHeight = 24.0f;

  // 入力ピンバー (上部)
  if (!node.inputPinIds.empty()) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 barMin = ImGui::GetCursorScreenPos();
    ImVec2 barMax = ImVec2(barMin.x + nodeWidth, barMin.y + barHeight);
    drawList->AddRectFilled(barMin, barMax, IM_COL32(30, 30, 30, 255), 3.0f);
  // ノードの順番番号表示
    int childOrder = GetChildOrder(node.id);
    if (childOrder > 0) {
      std::string orderText = std::to_string(childOrder);
      drawList->AddText(ImVec2(barMin.x + 6.0f, barMin.y + 4.0f),
        IM_COL32(255, 220, 80, 255), orderText.c_str());
    }

    ImGui::Dummy(ImVec2(nodeWidth, barHeight));

    ImVec2 savedPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(savedPos.x, savedPos.y - barHeight + 4));

    for (int pinId : node.inputPinIds) {
      const EditorPin* pin = FindPinById(pinId);
      if (pin) {
        float textWidth = ImGui::CalcTextSize("Input").x;
        ImGui::Dummy(ImVec2((nodeWidth - textWidth) * 0.5f, 0));
        ImGui::SameLine(0, 0);
        DrawPin(*pin);
      }
    }
    ImGui::SetCursorPos(savedPos);
  }

  // ノード本体 (中央)
  ImGui::Spacing();

  const char* titleText = node.displayName.c_str();
  float titleWidth = ImGui::CalcTextSize(titleText).x;
  ImGui::Dummy(ImVec2((nodeWidth - titleWidth) * 0.5f, 0));
  ImGui::SameLine(0, 0);
  ImGui::Text("%s", titleText);

  // ノードタイプを小さく表示 (中央揃え)
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
  ImGui::SetWindowFontScale(0.85f);
  float typeWidth = ImGui::CalcTextSize(node.nodeType.c_str()).x * 0.85f;
  ImGui::Dummy(ImVec2((nodeWidth - typeWidth) * 0.5f, 0));
  ImGui::SameLine(0, 0);
  ImGui::Text("%s", node.nodeType.c_str());
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor();

  ImGui::Spacing();

  // 出力ピンバー (下部)
  if (!node.outputPinIds.empty()) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 barMin = ImGui::GetCursorScreenPos();
    ImVec2 barMax = ImVec2(barMin.x + nodeWidth, barMin.y + barHeight);
    drawList->AddRectFilled(barMin, barMax, IM_COL32(30, 30, 30, 255), 3.0f);

    ImGui::Dummy(ImVec2(nodeWidth, barHeight));

    ImVec2 savedPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(savedPos.x, savedPos.y - barHeight + 4));

    int pinCount = static_cast<int>(node.outputPinIds.size());

    if (pinCount == 1 && !node.outputPinIds.empty()) {
      const EditorPin* pin = FindPinById(node.outputPinIds[0]);
      if (pin) {
        float textWidth = ImGui::CalcTextSize("Output").x;
        ImGui::Dummy(ImVec2((nodeWidth - textWidth) * 0.5f, 0));
        ImGui::SameLine(0, 0);
        DrawPin(*pin);
      }
    }
    else if (pinCount > 1 && node.outputPinIds.size() >= static_cast<size_t>(pinCount)) {
      float spacing = nodeWidth / (pinCount + 1);

      for (int i = 0; i < pinCount && i < static_cast<int>(node.outputPinIds.size()); i++) {
        const EditorPin* pin = FindPinById(node.outputPinIds[i]);
        if (pin) {
          float offset = spacing * (i + 1) - 20;
          ImGui::Dummy(ImVec2(offset, 0));
          ImGui::SameLine(0, 0);
          DrawPin(*pin);
          if (i < pinCount - 1) {
            ImGui::SameLine();
          }
        }
      }
    }

    ImGui::SetCursorPos(savedPos);
  }

  ImGui::PopID();
  ed::EndNode();

  ed::PopStyleVar(2);
  ed::PopStyleColor(2);
}

void BehaviorTreeEditor::DrawPin(const EditorPin& pin) {
  // ピンのカラー設定
  ImColor pinColor = pin.isInput ? ImColor(150, 150, 200) : ImColor(150, 200, 150);
  ImColor borderColor = ImColor(200, 200, 200, 200);

  ed::PushStyleColor(ed::StyleColor_PinRect, pinColor);
  ed::PushStyleColor(ed::StyleColor_PinRectBorder, borderColor);

  ed::BeginPin(pin.id, pin.isInput ? ed::PinKind::Input : ed::PinKind::Output);
  ImGui::PushID(pin.id);

  ImVec2 pinRectMin = ImGui::GetCursorScreenPos();
  ImGui::Text("    ");
  ImVec2 pinRectMax = ImVec2(
    ImGui::GetItemRectMax().x,
    ImGui::GetItemRectMax().y
  );

  ed::PinRect(pinRectMin, pinRectMax);

  // 入力ピンは上端、出力ピンは下端にアイコン配置
  ImVec2 alignment = pin.isInput ? ImVec2(0.5f, 0.0f) : ImVec2(0.5f, 1.0f);
  ed::PinPivotAlignment(alignment);
  ed::PinPivotSize(ImVec2(0, 0));

  ImGui::PopID();
  ed::EndPin();

  ed::PopStyleColor(2);
}

void BehaviorTreeEditor::DrawLinks() {
  for (const auto& link : links_) {
    ed::Link(link.id, link.startPinId, link.endPinId, ImColor(200, 200, 200), 2.0f);
  }
}

void BehaviorTreeEditor::HandleLinkCreation() {
  if (ed::BeginCreate()) {
    ed::PinId inputPinId, outputPinId;

    if (ed::QueryNewLink(&inputPinId, &outputPinId)) {
      EditorPin* inputPin = FindPinById(static_cast<int>(inputPinId.Get()));
      EditorPin* outputPin = FindPinById(static_cast<int>(outputPinId.Get()));

      if (inputPin && outputPin) {
        // 入力と出力が逆の場合は入れ替える
        if (inputPin->isInput == false && outputPin->isInput == true) {
          std::swap(inputPin, outputPin);
          std::swap(inputPinId, outputPinId);
        }

        bool canCreateLink = inputPin->isInput != outputPin->isInput;

        // 循環参照チェック
        if (canCreateLink) {
          canCreateLink = !HasCyclicDependency(outputPin->nodeId, inputPin->nodeId);
        }

        if (canCreateLink && ed::AcceptNewItem()) {
          EditorLink newLink;
          newLink.id = nextLinkId_++;
          newLink.startPinId = static_cast<int>(outputPinId.Get());
          newLink.endPinId = static_cast<int>(inputPinId.Get());
          newLink.startNodeId = outputPin->nodeId;
          newLink.endNodeId = inputPin->nodeId;

          links_.push_back(newLink);
        }
        else if (!canCreateLink) {
          // リンク作成を拒否 (赤色で表示)
          ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
        }
      }
    }
  }
  ed::EndCreate();
}

void BehaviorTreeEditor::HandleDeletion() {
  if (ed::BeginDelete()) {
    // リンクの削除
    ed::LinkId deletedLinkId;
    while (ed::QueryDeletedLink(&deletedLinkId)) {
      if (ed::AcceptDeletedItem()) {
        links_.erase(
          std::remove_if(links_.begin(), links_.end(),
            [deletedLinkId](const EditorLink& link) {
              return link.id == static_cast<int>(deletedLinkId.Get());
            }),
          links_.end()
        );
      }
    }

    // ノードの削除
    ed::NodeId deletedNodeId;
    while (ed::QueryDeletedNode(&deletedNodeId)) {
      if (ed::AcceptDeletedItem()) {
        RemoveNode(static_cast<int>(deletedNodeId.Get()));
      }
    }
  }
  ed::EndDelete();
}

void BehaviorTreeEditor::DrawContextMenu() {
  ed::Suspend();

  if (ed::ShowBackgroundContextMenu()) {
    ImGui::OpenPopup("CreateNodeMenu");
  }

  ed::NodeId contextNodeId;
  if (ed::ShowNodeContextMenu(&contextNodeId)) {
    ImGui::OpenPopup("NodeContextMenu");
    ImGui::SetNextWindowSize(ImVec2(200, 0));
  }

  if (ImGui::BeginPopup("CreateNodeMenu")) {
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 canvasPos = ed::ScreenToCanvas(mousePos);

    ImGui::Text("Create Node");
    ImGui::Separator();

    auto* registry = BTNodeRegistry::GetInstance();

    if (ImGui::BeginMenu("Composite Nodes")) {
      auto types = registry->GetTypesByCategory(NodeCategory::Composite);
      for (const auto& nodeType : types) {
        const NodeMeta* meta = registry->GetMeta(nodeType);
        std::string displayName = meta ? meta->displayName : nodeType;
        if (ImGui::MenuItem(displayName.c_str())) {
          CreateNode(nodeType, canvasPos);
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Action Nodes")) {
      auto types = registry->GetTypesByCategory(NodeCategory::Action);
      for (const auto& nodeType : types) {
        const NodeMeta* meta = registry->GetMeta(nodeType);
        std::string displayName = meta ? meta->displayName : nodeType;
        if (ImGui::MenuItem(displayName.c_str())) {
          CreateNode(nodeType, canvasPos);
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Condition Nodes")) {
      auto types = registry->GetTypesByCategory(NodeCategory::Condition);
      for (const auto& nodeType : types) {
        const NodeMeta* meta = registry->GetMeta(nodeType);
        std::string displayName = meta ? meta->displayName : nodeType;
        if (ImGui::MenuItem(displayName.c_str())) {
          CreateNode(nodeType, canvasPos);
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Decorator Nodes")) {
      auto types = registry->GetTypesByCategory(NodeCategory::Decorator);
      for (const auto& nodeType : types) {
        const NodeMeta* meta = registry->GetMeta(nodeType);
        std::string displayName = meta ? meta->displayName : nodeType;
        if (ImGui::MenuItem(displayName.c_str())) {
          CreateNode(nodeType, canvasPos);
        }
      }
      ImGui::EndMenu();
    }

    ImGui::EndPopup();
  }

  ed::Resume();
}

void BehaviorTreeEditor::DrawNodeInspector() {
  if (!ImGui::Begin(config_.nodeInspectorName.c_str())) {
    ImGui::End();
    return;
  }
  treeTracker_.BeginSection();

  ImGui::Text("Inspector");
  ImGui::Separator();

  if (selectedNodeId_ < 0) {
    ImGui::TextDisabled("No node selected");
    treeTracker_.EndSection();
    ImGui::End();
    return;
  }

  EditorNode* node = FindNodeById(selectedNodeId_);
  if (!node) {
    ImGui::TextDisabled("Invalid selection");
    treeTracker_.EndSection();
    ImGui::End();
    return;
  }

  ImGui::Text("ID: %d", node->id);
  ImGui::Text("Type: %s", node->nodeType.c_str());

  // 表示名編集
  char nameBuf[256];
  strncpy_s(nameBuf, node->displayName.c_str(), sizeof(nameBuf) - 1);
  if (ImGui::InputText("Name##inspector", nameBuf, sizeof(nameBuf))) {
    node->displayName = nameBuf;
  }

  ImGui::Separator();
  ImGui::Text("Parameters");

  // ノード自身の DrawImGui でパラメータ編集（編集は区間の前後の比較で見つける）
  if (node->runtimeNode) {
    if (!node->runtimeNode->DrawImGui()) {
      ImGui::TextDisabled("No editable parameters");
    }
  }
  else {
    ImGui::TextDisabled("No runtime node");
  }

  treeTracker_.EndSection();
  ImGui::End();
}

bool BehaviorTreeEditor::LoadFromJSON(const std::string& filepath) {
  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      DebugUIManager::GetInstance()->AddLog(
        "[BehaviorTreeEditor] Failed to open file for reading: " + filepath,
        DebugUIManager::LogType::Error);
      return false;
    }

    nlohmann::json json;
    file >> json;
    file.close();

    // バージョンチェック
    if (!json.contains("version") || json["version"] != "1.0") {
      DebugUIManager::GetInstance()->AddLog(
        "[BehaviorTreeEditor] Unsupported file version",
        DebugUIManager::LogType::Error);
      return false;
    }

    // 既存のエディタをクリア
    Clear();

    // ID マッピング (古い ID → 新しい ID)
    std::unordered_map<int, int> oldToNewNodeIdMap;

    // ノードを復元
    if (json.contains("nodes")) {
      for (const auto& nodeJson : json["nodes"]) {
        int oldId = nodeJson["id"];
        std::string nodeType = nodeJson["type"];
        std::string displayName = nodeJson.value("displayName", nodeType);
        ImVec2 position(
          nodeJson["position"]["x"],
          nodeJson["position"]["y"]
        );

        int newId = CreateNodeWithId(nextNodeId_++, nodeType, position);
        if (newId != -1) {
          oldToNewNodeIdMap[oldId] = newId;
          pendingNodePositions_.emplace_back(newId, position);  // JSON 位置をロード時に ed:: へ適用 

          auto* node = FindNodeById(newId);
          if (node) {
            node->displayName = displayName;

            if (nodeJson.contains("parameters")) {
              ApplyNodeParameters(*node, nodeJson["parameters"]);
            }
          }
        }
      }
    }

    // リンクを復元
    if (json.contains("links")) {
      for (const auto& linkJson : json["links"]) {
        int oldSourceNodeId = linkJson["sourceNodeId"];
        int oldTargetNodeId = linkJson["targetNodeId"];

        auto sourceIt = oldToNewNodeIdMap.find(oldSourceNodeId);
        auto targetIt = oldToNewNodeIdMap.find(oldTargetNodeId);

        if (sourceIt != oldToNewNodeIdMap.end() &&
          targetIt != oldToNewNodeIdMap.end()) {
          int newSourceNodeId = sourceIt->second;
          int newTargetNodeId = targetIt->second;

          CreateLink(newSourceNodeId, newTargetNodeId);
        }
      }
    }

    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Successfully loaded from: " + filepath,
      DebugUIManager::LogType::Info);
    DebugUIManager::GetInstance()->AddLog(
      std::format("[BehaviorTreeEditor] Loaded {} nodes and {} links", nodes_.size(), links_.size()),
      DebugUIManager::LogType::Info);

    // ロード直後にキャンバスのビューポートを全ノードへ自動フォーカス
    pendingNavigateToContent_ = true;
    treeTracker_.Reset();  // ロード直後は未保存の印と Undo の履歴を捨てる

    return true;
  }
  catch (const std::exception& e) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Failed to load JSON: " + std::string(e.what()),
      DebugUIManager::LogType::Error);
    return false;
  }
}

std::string BehaviorTreeEditor::SnapshotTree() {
  nlohmann::json tree = SerializeTree();
  for (nlohmann::json& link : tree["links"]) {
    link.erase("id");
    link.erase("sourcePinId");
    link.erase("targetPinId");
  }
  return tree.dump();
}

void BehaviorTreeEditor::RestoreTree(const std::string& tree) {
  const nlohmann::json json      = nlohmann::json::parse(tree);
  const nlohmann::json& nodesJson = json.at("nodes");

  std::vector<int> removedIds;
  for (const EditorNode& node : nodes_) {
    if (std::ranges::none_of(nodesJson, [&node](const nlohmann::json& nodeJson) { return nodeJson["id"] == node.id; })) {
      removedIds.push_back(node.id);
    }
  }
  for (int nodeId : removedIds) {
    RemoveNode(nodeId);
  }

  std::vector<int> nodeOrder;
  for (const nlohmann::json& nodeJson : nodesJson) {
    const int    nodeId   = nodeJson["id"].get<int>();
    const ImVec2 position = ImVec2(nodeJson["position"]["x"], nodeJson["position"]["y"]);
    EditorNode*  node     = FindNodeById(nodeId);
    if (!node) {
      if (CreateNodeWithId(nodeId, nodeJson["type"].get<std::string>(), position) == -1) {
        continue;
      }
      node = FindNodeById(nodeId);
      pendingNodePositions_.emplace_back(nodeId, node->position);
    }
    else if (node->position.x != position.x || node->position.y != position.y) {
      node->position = position;
      pendingNodePositions_.emplace_back(nodeId, position);
    }
    node->displayName = nodeJson.value("displayName", node->nodeType);
    ApplyNodeParameters(*node, nodeJson["parameters"]);
    nextNodeId_ = (std::max)(nextNodeId_, nodeId + 1);
    nodeOrder.push_back(nodeId);
  }

  // リンクはノード ID の組で合わせる
  std::vector<std::pair<int, int>> linkPairs;
  for (const nlohmann::json& linkJson : json.at("links")) {
    linkPairs.emplace_back(linkJson["sourceNodeId"].get<int>(), linkJson["targetNodeId"].get<int>());
  }
  const auto linkOrder = [&linkPairs](const EditorLink& link) {
    return std::ranges::find(linkPairs, std::pair(link.startNodeId, link.endNodeId)) - linkPairs.begin();
  };
  std::erase_if(links_, [&](const EditorLink& link) { return std::ranges::find(linkPairs, std::pair(link.startNodeId, link.endNodeId)) == linkPairs.end(); });
  for (const auto& [sourceId, targetId] : linkPairs) {
    if (std::ranges::none_of(links_, [&](const EditorLink& link) { return link.startNodeId == sourceId && link.endNodeId == targetId; })) {
      CreateLink(sourceId, targetId);
    }
  }

  // 戻した内容を比べたときに元のスナップショットと同じになるよう、並びもそろえる
  std::ranges::sort(nodes_, {}, [&nodeOrder](const EditorNode& node) { return std::ranges::find(nodeOrder, node.id) - nodeOrder.begin(); });
  std::ranges::sort(links_, {}, linkOrder);
}

void BehaviorTreeEditor::RemoveNode(int nodeId) {
  std::erase_if(links_, [nodeId](const EditorLink& link) { return link.startNodeId == nodeId || link.endNodeId == nodeId; });
  std::erase_if(pins_, [nodeId](const EditorPin& pin) { return pin.nodeId == nodeId; });
  std::erase_if(runtimeNodeToEditorId_, [nodeId](const auto& entry) { return entry.second == nodeId; });
  std::erase_if(nodes_, [nodeId](const EditorNode& node) { return node.id == nodeId; });
  parameterHashes_.erase(nodeId);
}

uint64_t BehaviorTreeEditor::HashTree() {
  if (const EditorNode* selected = FindNodeById(selectedNodeId_)) {
    UpdateParameterHash(*selected);
  }
  Fnv1aHasher hash;
  hash.Add(nodes_.size());
  for (const EditorNode& node : nodes_) {
    hash.Add(node.id);
    hash.Add(node.nodeType);
    hash.Add(node.displayName);
    hash.Add(node.position);
    hash.Add(parameterHashes_[node.id]);
  }
  hash.Add(links_.size());
  for (const EditorLink& link : links_) {
    hash.Add(link.startNodeId);
    hash.Add(link.endNodeId);
  }
  return hash.Get();
}

void BehaviorTreeEditor::UpdateParameterHash(const EditorNode& node) {
  Fnv1aHasher hash;
  hash.Add(ExtractNodeParameters(node).dump());
  parameterHashes_[node.id] = hash.Get();
}

nlohmann::json BehaviorTreeEditor::SerializeTree() {
  nlohmann::json json;

  // 位置は DrawNodes が毎フレーム ed から写しているもの（追加直後で ed にまだ位置が無いノードは予約した位置）
  json["nodes"] = nlohmann::json::array();
  for (const auto& node : nodes_) {
    nlohmann::json nodeJson;
    nodeJson["id"] = node.id;
    nodeJson["type"] = node.nodeType;
    nodeJson["displayName"] = node.displayName;
    nodeJson["position"]["x"] = node.position.x;
    nodeJson["position"]["y"] = node.position.y;
    nodeJson["parameters"] = ExtractNodeParameters(node);
    json["nodes"].push_back(nodeJson);
  }

  json["links"] = nlohmann::json::array();
  for (const auto& link : links_) {
    nlohmann::json linkJson;
    linkJson["id"] = link.id;
    linkJson["sourceNodeId"] = link.startNodeId;
    linkJson["targetNodeId"] = link.endNodeId;
    linkJson["sourcePinId"] = link.startPinId;
    linkJson["targetPinId"] = link.endPinId;
    json["links"].push_back(linkJson);
  }
  return json;
}

bool BehaviorTreeEditor::SaveToJSON(const std::string& filepath) {
  try {
    const nlohmann::json tree = SerializeTree();
    nlohmann::json       json = tree;

    json["version"] = "1.0";

    // メタデータ
    auto now = std::chrono::system_clock::now();
    auto localTime = std::chrono::current_zone()->to_local(now);
    std::string timestamp = std::format("{:%Y-%m-%dT%H:%M:%S}",
      std::chrono::floor<std::chrono::seconds>(localTime));

    json["metadata"]["name"] = "Behavior Tree";
    json["metadata"]["created"] = timestamp;
    json["metadata"]["modified"] = timestamp;

    // ディレクトリが存在しない場合は作成
    std::filesystem::path filePath(filepath);
    std::filesystem::create_directories(filePath.parent_path());

    std::ofstream file(filepath);
    if (!file.is_open()) {
      DebugUIManager::GetInstance()->AddLog(
        "[BehaviorTreeEditor] Failed to open file for writing: " + filepath,
        DebugUIManager::LogType::Error);
      return false;
    }

    file << json.dump(2);  // インデント 2 で整形
    file.close();

    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Successfully saved to: " + filepath,
      DebugUIManager::LogType::Info);
    treeTracker_.MarkSaved();  // 保存直後は未保存の印を外す
    return true;
  }
  catch (const std::exception& e) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Failed to save JSON: " + std::string(e.what()),
      DebugUIManager::LogType::Error);
    return false;
  }
}

BTNodePtr BehaviorTreeEditor::BuildRuntimeTree() {
  if (nodes_.empty()) {
    return nullptr;
  }

  int rootId = FindRootNodeId();
  if (rootId == -1) {
    // フォールバック: 最初のノードをルートとする
    rootId = nodes_[0].id;
  }

  BTNodePtr rootNode;
  BuildRuntimeTreeRecursive(rootId, rootNode);
  return rootNode;
}

void BehaviorTreeEditor::HighlightRunningNode(const BTNodePtr& nodePtr) {
  if (!nodePtr) {
    highlightedNodeId_ = -1;
    return;
  }

  EditorNode* editorNode = FindNodeByRuntimeNode(nodePtr);
  if (editorNode) {
    if (highlightedNodeId_ != editorNode->id) {
      highlightedNodeId_ = editorNode->id;
      highlightStartTime_ = static_cast<float>(ImGui::GetTime());
    }
  }
}

//==========================================================================
// 検索ヘルパー
//==========================================================================

EditorNode* BehaviorTreeEditor::FindNodeById(int nodeId) {
  for (auto& node : nodes_) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

const EditorNode* BehaviorTreeEditor::FindNodeById(int nodeId) const {
  for (const auto& node : nodes_) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

EditorNode* BehaviorTreeEditor::FindNodeByRuntimeNode(const BTNodePtr& node) {
  if (!node) return nullptr;
  auto it = runtimeNodeToEditorId_.find(node.get());
  if (it != runtimeNodeToEditorId_.end()) {
    return FindNodeById(it->second);
  }
  return nullptr;
}

EditorPin* BehaviorTreeEditor::FindPinById(int pinId) {
  for (auto& pin : pins_) {
    if (pin.id == pinId) {
      return &pin;
    }
  }
  return nullptr;
}

const EditorPin* BehaviorTreeEditor::FindPinById(int pinId) const {
  for (const auto& pin : pins_) {
    if (pin.id == pinId) {
      return &pin;
    }
  }
  return nullptr;
}

//==========================================================================
// ツリー構築・解析
//==========================================================================

int BehaviorTreeEditor::FindRootNodeId() const {
  // 入力リンクを持たないノードを探す (ルート)
  for (const auto& node : nodes_) {
    bool hasInputLink = false;
    for (const auto& link : links_) {
      if (link.endNodeId == node.id) {
        hasInputLink = true;
        break;
      }
    }
    if (!hasInputLink) {
      return node.id;
    }
  }
  return -1;
}

std::vector<int> BehaviorTreeEditor::GetChildNodeIds(int parentNodeId) const {
  std::vector<int> childIds;
  for (const auto& link : links_) {
    if (link.startNodeId == parentNodeId) {
      childIds.push_back(link.endNodeId);
    }
  }
  SortChildIdsByPosition(childIds, [this](int id, float& x, float& y) {
    const EditorNode* n = FindNodeById(id);
    if (!n) return false;
    x = n->position.x;
    y = n->position.y;
    return true;
  });
  return childIds;
}

int BehaviorTreeEditor::GetChildOrder(int nodeId) const {
  for (const auto& link : links_) {
    if (link.endNodeId == nodeId) {
      std::vector<int> siblings = GetChildNodeIds(link.startNodeId);
      for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i] == nodeId) {
          return static_cast<int>(i) + 1;
        }
      }
    }
  }
  return -1;
}

bool BehaviorTreeEditor::HasCyclicDependency(int startNodeId, int endNodeId) const {
  // startNodeId から endNodeId へリンクを作成した場合、循環するか BFS でチェック
  std::set<int> visited;
  std::queue<int> queue;
  queue.push(endNodeId);

  while (!queue.empty()) {
    int currentId = queue.front();
    queue.pop();

    if (currentId == startNodeId) {
      return true;
    }

    if (visited.find(currentId) != visited.end()) {
      continue;
    }
    visited.insert(currentId);

    std::vector<int> childIds = GetChildNodeIds(currentId);
    for (int childId : childIds) {
      queue.push(childId);
    }
  }
  return false;
}

void BehaviorTreeEditor::CreateNode(const std::string& nodeType, const ImVec2& position) {
  int newId = CreateNodeWithId(nextNodeId_++, nodeType, position);
  // 新規ノードのみ次フレームの ed::Begin 内で ed::SetNodePosition を呼びたいので予約。
  // (Add Node ボタンは ed::Begin の外で押されるため、即時 SetNodePosition は効かない)
  if (newId != -1) {
    pendingNodePositions_.emplace_back(newId, position);
  }
}

int BehaviorTreeEditor::CreateNodeWithId(int nodeId, const std::string& nodeType, const ImVec2& position) {
  auto* registry = BTNodeRegistry::GetInstance();
  const NodeMeta* meta = registry->GetMeta(nodeType);

  EditorNode newNode;
  newNode.id = nodeId;
  // ed は SetNodePosition で位置を 0 方向へ切り捨てる（ImRect::Floor）。同じ値にして、位置の適用を編集と見なさないようにする
  newNode.position = ImVec2(std::trunc(position.x), std::trunc(position.y));
  newNode.nodeType = nodeType;
  newNode.displayName = meta ? meta->displayName : nodeType;
  // NodeMeta::color (NodeColor) を ImGui の ImVec4 に変換 (エディタ描画用)
  newNode.color = meta
    ? ImVec4(meta->color.r, meta->color.g, meta->color.b, meta->color.a)
    : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

  // ランタイムノードを生成
  newNode.runtimeNode = registry->Create(nodeType);
  if (!newNode.runtimeNode) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Failed to create runtime node for type: " + nodeType,
      DebugUIManager::LogType::Warning);
    return -1;
  }

  // 入力ピンを作成 (全ノードは親を持てる)
  EditorPin inputPin;
  inputPin.id = nextPinId_++;
  inputPin.nodeId = nodeId;
  inputPin.isInput = true;
  inputPin.name = "In";
  pins_.push_back(inputPin);
  newNode.inputPinIds.push_back(inputPin.id);

  // 出力ピンを作成 (コンポジットノードのみ)
  bool isComposite = meta ? meta->isComposite : false;
  if (isComposite) {
    EditorPin outputPin;
    outputPin.id = nextPinId_++;
    outputPin.nodeId = nodeId;
    outputPin.isInput = false;
    outputPin.name = "Out";
    pins_.push_back(outputPin);
    newNode.outputPinIds.push_back(outputPin.id);
  }

  nodes_.push_back(newNode);
  UpdateParameterHash(nodes_.back());

  // ランタイムノードとエディタ ID のマッピング
  if (newNode.runtimeNode) {
    runtimeNodeToEditorId_[newNode.runtimeNode.get()] = nodeId;
  }

  return nodeId;
}

void BehaviorTreeEditor::BuildRuntimeTreeRecursive(int nodeId, BTNodePtr& outNode) {
  EditorNode* editorNode = FindNodeById(nodeId);
  if (!editorNode || !editorNode->runtimeNode) {
    return;
  }

  outNode = editorNode->runtimeNode;

  // コンポジットノードの場合、子ノードを追加
  auto* registry = BTNodeRegistry::GetInstance();
  const NodeMeta* meta = registry->GetMeta(editorNode->nodeType);
  bool isComposite = meta ? meta->isComposite : false;

  if (isComposite) {
    auto compositeNode = std::dynamic_pointer_cast<BTComposite>(outNode);
    if (compositeNode) {
      compositeNode->ClearChildren();
      std::vector<int> childIds = GetChildNodeIds(nodeId);
      for (int childId : childIds) {
        BTNodePtr childNode;
        BuildRuntimeTreeRecursive(childId, childNode);
        if (childNode) {
          compositeNode->AddChild(childNode);
        }
      }
    }
  }
}

nlohmann::json BehaviorTreeEditor::ExtractNodeParameters(const EditorNode& node) {
  if (!node.runtimeNode) return {};
  return node.runtimeNode->ExtractParameters();
}

void BehaviorTreeEditor::ApplyNodeParameters(EditorNode& node, const nlohmann::json& params) {
  if (!node.runtimeNode || params.empty()) return;
  node.runtimeNode->ApplyParameters(params);
  UpdateParameterHash(node);
}

//==========================================================================
// マルチツリー API (treeName ベース)
//==========================================================================

std::string BehaviorTreeEditor::GetTreeFilePath(const std::string& treeName) const {
  return config_.btJsonDir + treeName + ".json";
}

bool BehaviorTreeEditor::LoadTree(const std::string& treeName) {
  currentTreeName_ = treeName;
  bool ok = LoadFromJSON(GetTreeFilePath(treeName));
  // 未保存の印は LoadFromJSON 内で外している
  return ok;
}

bool BehaviorTreeEditor::SaveTree(const std::string& treeName) {
  bool ok = SaveToJSON(GetTreeFilePath(treeName));
  if (ok) {
    currentTreeName_ = treeName;
    // 未保存の印は SaveToJSON 内で外している
  }
  return ok;
}

bool BehaviorTreeEditor::CreateNewTree(const std::string& treeName) {
  if (treeName.empty() || treeName[0] == '_') {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] CreateNewTree: invalid name (empty or underscore-prefixed): " + treeName,
      DebugUIManager::LogType::Error);
    return false;
  }
  const std::string treePath = GetTreeFilePath(treeName);
  if (std::filesystem::exists(treePath)) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] CreateNewTree: file already exists: " + treePath,
      DebugUIManager::LogType::Warning);
    return false;
  }
  // 現在の編集状態と Undo の履歴をクリアし、新規ツリーを保存
  Clear();
  treeTracker_.Reset();
  currentTreeName_ = treeName;
  return SaveToJSON(treePath);
}

bool BehaviorTreeEditor::CreateTreeFromCopy(const std::string& sourceTreeName, const std::string& newTreeName) {
  if (newTreeName.empty() || newTreeName[0] == '_') {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] CreateTreeFromCopy: invalid name (empty or underscore-prefixed): " + newTreeName,
      DebugUIManager::LogType::Error);
    return false;
  }
  const std::string newPath = GetTreeFilePath(newTreeName);
  if (std::filesystem::exists(newPath)) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] CreateTreeFromCopy: file already exists: " + newPath,
      DebugUIManager::LogType::Warning);
    return false;
  }
  const std::string sourcePath = GetTreeFilePath(sourceTreeName);
  if (!std::filesystem::exists(sourcePath)) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] CreateTreeFromCopy: source not found: " + sourcePath,
      DebugUIManager::LogType::Error);
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::copy_file(sourcePath, newPath, ec)) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] CreateTreeFromCopy: copy failed: " + ec.message(),
      DebugUIManager::LogType::Error);
    return false;
  }
  return LoadTree(newTreeName);  // 複製したツリーへ切替
}

bool BehaviorTreeEditor::DeleteTree(const std::string& treeName) {
  if (treeName == currentTreeName_) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] DeleteTree: cannot delete currently editing tree: " + treeName,
      DebugUIManager::LogType::Warning);
    return false;
  }
  const std::string treePath = GetTreeFilePath(treeName);
  std::error_code ec;
  bool ok = std::filesystem::remove(treePath, ec);
  if (ok) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] DeleteTree: removed tree " + treePath,
      DebugUIManager::LogType::Info);
  }
  else {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] DeleteTree: failed to remove " + treePath + " (" + ec.message() + ")",
      DebugUIManager::LogType::Error);
  }
  return ok;
}

void BehaviorTreeEditor::SwitchTree(const std::string& treeName) {
  if (treeName == currentTreeName_) return;
  RunOrConfirm("Switch", [this, treeName] { return LoadTree(treeName); });
}

bool BehaviorTreeEditor::RunOrConfirm(const std::string& actionName, std::function<bool()> action) {
  if (!treeTracker_.IsEdited()) {
    return action();
  }
  unsavedPrompt_.RunOrConfirm(true, std::format("Tree '{}' has unsaved changes.", currentTreeName_), actionName, [action = std::move(action)] { action(); });
  return true;
}

std::vector<std::string> BehaviorTreeEditor::ListAvailableTrees() const {
  std::vector<std::string> result;
  std::error_code ec;
  if (!std::filesystem::exists(config_.btJsonDir, ec)) {
    return result;
  }
  for (const auto& entry : std::filesystem::directory_iterator(config_.btJsonDir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".json") continue;
    std::string stem = entry.path().stem().string();
    // アンダースコア prefix のファイルは予約名として除外
    if (stem.empty() || stem[0] == '_') continue;
    result.push_back(stem);
  }
  std::sort(result.begin(), result.end());
  return result;
}

//==========================================================================
// 既存の CreateLink (汎用ヘルパー)
//==========================================================================

bool BehaviorTreeEditor::CreateLink(int sourceNodeId, int targetNodeId) {
  auto* sourceNode = FindNodeById(sourceNodeId);
  auto* targetNode = FindNodeById(targetNodeId);

  if (!sourceNode || !targetNode) {
    return false;
  }
  if (sourceNode->outputPinIds.empty() || targetNode->inputPinIds.empty()) {
    return false;
  }

  EditorLink link;
  link.id = nextLinkId_++;
  link.startPinId = sourceNode->outputPinIds[0];
  link.endPinId = targetNode->inputPinIds[0];
  link.startNodeId = sourceNodeId;
  link.endNodeId = targetNodeId;
  links_.push_back(link);
  return true;
}

} // namespace Tako

#endif // _DEBUG
