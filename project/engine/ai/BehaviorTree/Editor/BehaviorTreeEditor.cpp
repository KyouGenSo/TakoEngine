#ifdef _DEBUG

#include "BehaviorTreeEditor.h"
#include "BTComposite.h"
#include "BTBlackboard.h"
#include "BTNodeRegistry.h"
#include "DebugUIManager.h"
#include <imgui_internal.h>
#include <algorithm>
#include <queue>
#include <set>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <format>
#include <filesystem>

namespace Tako {

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

  currentTreeName_ = config_.initialTreeFile.empty()
    ? "default"
    : std::filesystem::path(config_.initialTreeFile).stem().string();

  // SettingsFile はヒープアロケート回避のため static にキャッシュ (Config が const char* で保持するため)
  static std::string settingsFilePath;
  settingsFilePath = GetLayoutFilePath(currentTreeName_);
  editorConfig_->SettingsFile = settingsFilePath.c_str();

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
  // ツリー切替時に SettingsFile を変える必要があるため EditorContext を再作成
  if (pendingRebuildEditorContext_) {
    RebuildEditorContext();
    pendingRebuildEditorContext_ = false;
  }

  if (!isVisible_) return;

  if (ImGui::Begin(config_.windowName.c_str(), &isVisible_)) {
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

    DrawNodeInspector();

    if (firstFrame_) {
      firstFrame_ = false;
    }
  }
  ImGui::End();
}

void BehaviorTreeEditor::Clear() {
  nodes_.clear();
  pins_.clear();
  links_.clear();
  runtimeNodeToEditorId_.clear();
  pendingNodePositions_.clear();

  // ID 範囲はノード 10000 / ピン 20000 / リンク 30000 で分離 (imgui-node-editor の ID 競合回避)
  nextNodeId_ = 10000;
  nextPinId_ = 20000;
  nextLinkId_ = 30000;

  highlightedNodeId_ = -1;
  highlightStartTime_ = 0.0f;
  selectedNodeId_ = -1;
  firstFrame_ = true;
  pendingNavigateToContent_ = true;
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

  // === マルチツリー: 切替ドロップダウン + 新規/削除 ===
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::Text("Tree:");
  ImGui::SameLine();

  std::vector<std::string> availableTrees = ListAvailableTrees();
  // 未保存マークを付けて currentTreeName を表示
  std::string comboPreview = currentTreeName_;
  if (hasUnsavedChanges_) comboPreview += " *";

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
  if (ImGui::BeginPopupModal("New Tree##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter new tree name (no extension, no underscore prefix):");
    ImGui::InputText("##NewTreeName", newTreeNameBuf, sizeof(newTreeNameBuf));
    ImGui::Separator();
    if (ImGui::Button("Create##new")) {
      std::string name = newTreeNameBuf;
      if (!name.empty() && CreateNewTree(name)) {
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

  // 未保存変更モーダル (SwitchTree から発動)
  if (showUnsavedChangesModal_) {
    ImGui::OpenPopup("Unsaved Changes##modal");
    showUnsavedChangesModal_ = false;
  }
  if (ImGui::BeginPopupModal("Unsaved Changes##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Tree '%s' has unsaved changes.", currentTreeName_.c_str());
    ImGui::Text("Switch to '%s'?", pendingSwitchTarget_.c_str());
    ImGui::Separator();
    if (ImGui::Button("Save and Switch##unsaved")) {
      // 保存成功時のみ切替。失敗 (書き込み権限不足・ディスクエラー等) ならモーダル維持で再操作可能
      if (SaveTree(currentTreeName_)) {
        LoadTree(pendingSwitchTarget_);
        pendingSwitchTarget_.clear();
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard and Switch##unsaved")) {
      LoadTree(pendingSwitchTarget_);
      pendingSwitchTarget_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel##unsaved")) {
      pendingSwitchTarget_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

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
  for (const auto& node : nodes_) {
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
          hasUnsavedChanges_ = true;
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
        hasUnsavedChanges_ = true;
      }
    }

    // ノードの削除
    ed::NodeId deletedNodeId;
    while (ed::QueryDeletedNode(&deletedNodeId)) {
      if (ed::AcceptDeletedItem()) {
        int nodeId = static_cast<int>(deletedNodeId.Get());

        // ノードに関連するリンクを削除
        links_.erase(
          std::remove_if(links_.begin(), links_.end(),
            [nodeId](const EditorLink& link) {
              return link.startNodeId == nodeId || link.endNodeId == nodeId;
            }),
          links_.end()
        );

        // ノードのピンを削除
        EditorNode* node = FindNodeById(nodeId);
        if (node) {
          for (int pinId : node->inputPinIds) {
            pins_.erase(
              std::remove_if(pins_.begin(), pins_.end(),
                [pinId](const EditorPin& pin) {
                  return pin.id == pinId;
                }),
              pins_.end()
            );
          }
          for (int pinId : node->outputPinIds) {
            pins_.erase(
              std::remove_if(pins_.begin(), pins_.end(),
                [pinId](const EditorPin& pin) {
                  return pin.id == pinId;
                }),
              pins_.end()
            );
          }
        }

        // ノード自体を削除
        nodes_.erase(
          std::remove_if(nodes_.begin(), nodes_.end(),
            [nodeId](const EditorNode& node) {
              return node.id == nodeId;
            }),
          nodes_.end()
        );
        hasUnsavedChanges_ = true;
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

  ImGui::Text("Inspector");
  ImGui::Separator();

  if (selectedNodeId_ < 0) {
    ImGui::TextDisabled("No node selected");
    ImGui::End();
    return;
  }

  EditorNode* node = FindNodeById(selectedNodeId_);
  if (!node) {
    ImGui::TextDisabled("Invalid selection");
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

  // ノード自身の DrawImGui でパラメータ編集
  if (node->runtimeNode) {
    if (!node->runtimeNode->DrawImGui()) {
      ImGui::TextDisabled("No editable parameters");
    }
  }
  else {
    ImGui::TextDisabled("No runtime node");
  }

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

    // ロード直後のフレームで ed::SetNodePosition を確実に走らせるために firstFrame_ を立て直す。
    // 加えてキャンバスのビューポートを全ノードに自動フォーカスする。
    firstFrame_ = true;
    pendingNavigateToContent_ = true;
    hasUnsavedChanges_ = false;  // ロード直後は未保存状態をクリア

    return true;
  }
  catch (const std::exception& e) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] Failed to load JSON: " + std::string(e.what()),
      DebugUIManager::LogType::Error);
    return false;
  }
}

bool BehaviorTreeEditor::SaveToJSON(const std::string& filepath) {
  try {
    nlohmann::json json;

    json["version"] = "1.0";

    // メタデータ
    auto now = std::chrono::system_clock::now();
    auto localTime = std::chrono::current_zone()->to_local(now);
    std::string timestamp = std::format("{:%Y-%m-%dT%H:%M:%S}",
      std::chrono::floor<std::chrono::seconds>(localTime));

    json["metadata"]["name"] = "Behavior Tree";
    json["metadata"]["created"] = timestamp;
    json["metadata"]["modified"] = timestamp;

    // ノード情報を保存 (位置は imgui-node-editor の現在状態から取得して JSON と同期)
    ed::SetCurrentEditor(editorContext_);
    json["nodes"] = nlohmann::json::array();
    for (const auto& node : nodes_) {
      nlohmann::json nodeJson;
      nodeJson["id"] = node.id;
      nodeJson["type"] = node.nodeType;
      nodeJson["displayName"] = node.displayName;
      // ユーザーがドラッグで動かした最新位置を ed から取得
      ImVec2 currentPos = ed::GetNodePosition(node.id);
      nodeJson["position"]["x"] = currentPos.x;
      nodeJson["position"]["y"] = currentPos.y;
      nodeJson["parameters"] = ExtractNodeParameters(node);
      json["nodes"].push_back(nodeJson);
    }
    ed::SetCurrentEditor(nullptr);

    // リンク情報を保存
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
    hasUnsavedChanges_ = false;  // 保存直後は未保存状態をクリア
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
  return childIds;
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
    hasUnsavedChanges_ = true;
  }
}

int BehaviorTreeEditor::CreateNodeWithId(int nodeId, const std::string& nodeType, const ImVec2& position) {
  auto* registry = BTNodeRegistry::GetInstance();
  const NodeMeta* meta = registry->GetMeta(nodeType);

  EditorNode newNode;
  newNode.id = nodeId;
  newNode.position = position;
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
}

//==========================================================================
// マルチツリー API (treeName ベース)
//==========================================================================

std::string BehaviorTreeEditor::GetTreeFilePath(const std::string& treeName) const {
  return config_.btJsonDir + treeName + ".json";
}

std::string BehaviorTreeEditor::GetLayoutFilePath(const std::string& treeName) const {
  // アンダースコア prefix + "_layout" サフィックスで ListAvailableTrees から自動除外される命名規則
  return config_.btJsonDir + "_" + treeName + "_layout.json";
}

void BehaviorTreeEditor::RebuildEditorContext() {
  // 既存 EditorContext を破棄
  if (editorContext_) {
    ed::SetCurrentEditor(editorContext_);
    ed::DestroyEditor(editorContext_);
    editorContext_ = nullptr;
  }
  // SettingsFile を currentTreeName_ 対応の layout ファイルで更新
  // (毎回ヒープアロケートを避けるため static にキャッシュ。Config が SettingsFile への const char* を保持するため寿命管理が必要)
  static std::string settingsFilePath;
  settingsFilePath = GetLayoutFilePath(currentTreeName_);
  if (!editorConfig_) {
    editorConfig_ = std::make_unique<ed::Config>();
    editorConfig_->NavigateButtonIndex = 1;
    editorConfig_->ContextMenuButtonIndex = 2;
  }
  editorConfig_->SettingsFile = settingsFilePath.c_str();
  // 新しい SettingsFile で EditorContext 再作成
  editorContext_ = ed::CreateEditor(editorConfig_.get());
}

bool BehaviorTreeEditor::LoadTree(const std::string& treeName) {
  // 先に currentTreeName_ を更新し、次フレームで対応する layout に切替
  currentTreeName_ = treeName;
  pendingRebuildEditorContext_ = true;
  bool ok = LoadFromJSON(GetTreeFilePath(treeName));
  // hasUnsavedChanges_ は LoadFromJSON 内で false にされている
  return ok;
}

bool BehaviorTreeEditor::SaveTree(const std::string& treeName) {
  bool ok = SaveToJSON(GetTreeFilePath(treeName));
  if (ok) {
    currentTreeName_ = treeName;
    // hasUnsavedChanges_ は SaveToJSON 内で false にされている
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
  // 現在の編集状態をクリアし、新規ツリーを保存
  Clear();
  currentTreeName_ = treeName;
  pendingRebuildEditorContext_ = true;  // 新ツリー専用の layout ファイルへ切替
  return SaveToJSON(treePath);
}

bool BehaviorTreeEditor::DeleteTree(const std::string& treeName) {
  if (treeName == currentTreeName_) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] DeleteTree: cannot delete currently editing tree: " + treeName,
      DebugUIManager::LogType::Warning);
    return false;
  }
  const std::string treePath = GetTreeFilePath(treeName);
  const std::string layoutPath = GetLayoutFilePath(treeName);
  std::error_code ec;
  bool ok = std::filesystem::remove(treePath, ec);
  // layout ペアも削除 (存在しない・失敗しても無視 - ペア層は補助的)
  std::error_code ecLayout;
  std::filesystem::remove(layoutPath, ecLayout);
  if (ok) {
    DebugUIManager::GetInstance()->AddLog(
      "[BehaviorTreeEditor] DeleteTree: removed tree " + treePath + " (+ paired layout if exists)",
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
  if (hasUnsavedChanges_) {
    // 未保存変更あり → 確認モーダルを次フレームで表示
    pendingSwitchTarget_ = treeName;
    showUnsavedChangesModal_ = true;
  }
  else {
    LoadTree(treeName);
  }
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
    // アンダースコア prefix のファイル (例: _editor_layout.json) は予約名として除外
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
