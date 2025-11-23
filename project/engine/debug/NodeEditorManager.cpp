#ifdef _DEBUG

#include "NodeEditorManager.h"
#include <imgui.h>
#include <imgui_node_editor.h>
#include <algorithm>
#include <cmath>

// 静的メンバーの定義
NodeEditorManager* NodeEditorManager::instance_ = nullptr;

NodeEditorManager::~NodeEditorManager() {
  Finalize();
}

void NodeEditorManager::Initialize() {
  // エディタコンフィグの作成
  config_ = new ed::Config();
  config_->SettingsFile = "NodeEditor.json";
  config_->NavigateButtonIndex = 1; // マウス中ボタンでナビゲート
  config_->ContextMenuButtonIndex = 2; // マウス右ボタンでコンテキストメニュー

  // エディタコンテキストの作成
  context_ = ed::CreateEditor(config_);

  // サンプルグラフの作成（デバッグ用）
  CreateSampleGraph();
}

void NodeEditorManager::Finalize() {
  if (context_) {
    ed::DestroyEditor(context_);
    context_ = nullptr;
  }
  if (config_) {
    delete config_;
    config_ = nullptr;
  }
  Clear();
}

void NodeEditorManager::Update() {
  // 必要に応じて更新処理を追加
  if (autoArrange_) {
    AutoArrangeNodes();
  }
}

void NodeEditorManager::Draw() {
  if (!isVisible_) return;

  // ImGuiウィンドウの開始
  if (ImGui::Begin("Node Editor", &isVisible_)) {
    // ツールバーの描画（各ボタンにユニークIDを付与）
    if (ImGui::Button("Add Node##toolbar_add")) {
      CreateNode("New Node", 100.0f, 100.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All##toolbar_clear")) {
      Clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show Grid##toolbar_grid", &showGrid_);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Arrange##toolbar_arrange", &autoArrange_);
    ImGui::SameLine();
    if (ImGui::Button("Sample Graph##toolbar_sample")) {
      Clear();
      CreateSampleGraph();
    }

    ImGui::Separator();

    // ノードエディタキャンバスの開始
    ed::SetCurrentEditor(context_);
    ed::Begin("Node Editor Canvas");

    // ノードの描画
    DrawNodes();

    // リンクの描画
    DrawLinks();

    // インタラクション処理
    HandleCreation();
    HandleDeletion();

    ed::End();
    ed::SetCurrentEditor(nullptr);

    // 初回フレームの処理完了
    if (firstFrame_) {
      firstFrame_ = false;
    }
  }
  ImGui::End();
}

void NodeEditorManager::DrawNodes() {
  for (const auto& node : nodes_) {
    DrawNode(node);
  }
}

void NodeEditorManager::DrawNode(const Node& node) {
  // ノードの開始
  ed::BeginNode(node.id);

  // ImGuiのIDスコープを追加（エディターのIDスコープとは別に必要）
  ImGui::PushID(node.id);

  // シンプルなテキスト表示
  ImGui::Text("%s", node.name.c_str());
  ImGui::SameLine();
  ImGui::Text("[ID:%d]", node.id - 1000);

  // 入力ピン
  for (int pinId : node.inputPins) {
    Pin* pin = FindPin(pinId);
    if (pin) {
      DrawPin(*pin);
    }
  }

  ImGui::SameLine();
  ImGui::Dummy(ImVec2(100, 0)); // ノード内のスペース
  ImGui::SameLine();

  // 出力ピン
  for (int pinId : node.outputPins) {
    Pin* pin = FindPin(pinId);
    if (pin) {
      DrawPin(*pin);
    }
  }

  // ImGuiのIDスコープを終了
  ImGui::PopID();

  ed::EndNode();

  // 初回フレームのみノード位置を設定（ユーザーの移動を上書きしないため）
  if (firstFrame_ && (node.posX != 0 || node.posY != 0)) {
    ed::SetNodePosition(node.id, ImVec2(node.posX, node.posY));
  }
}

void NodeEditorManager::DrawPin(const Pin& pin) {
  // ピンの色を決定
  ImColor pinColor = pin.isInput ? ImColor(255, 128, 128) : ImColor(128, 255, 128);

  ed::BeginPin(pin.id, pin.isInput ? ed::PinKind::Input : ed::PinKind::Output);

  // ImGuiのIDスコープを追加（複数のピンが同じ名前を持つ場合の衝突を防ぐ）
  ImGui::PushID(&pin);

  // ピンアイコンの描画
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  float radius = 5.0f;

  // テキスト表示
  if (pin.isInput) {
    drawList->AddCircleFilled(ImVec2(pos.x - 8, pos.y + 8), radius, pinColor);
    ImGui::TextUnformatted(pin.name.c_str());
  }
  else {
    ImGui::TextUnformatted(pin.name.c_str());
    drawList->AddCircleFilled(ImVec2(pos.x + ImGui::CalcTextSize(pin.name.c_str()).x + 8, pos.y + 8), radius, pinColor);
  }

  // ImGuiのIDスコープを終了
  ImGui::PopID();

  ed::EndPin();
}

void NodeEditorManager::DrawLinks() {
  for (const auto& link : links_) {
    ed::Link(link.id, link.startPinId, link.endPinId, ImColor(200, 200, 200), 2.0f);
  }
}

void NodeEditorManager::HandleCreation() {
  if (ed::BeginCreate()) {
    ed::PinId inputPinId, outputPinId;
    if (ed::QueryNewLink(&inputPinId, &outputPinId)) {
      // ピンの検証
      Pin* inputPin = FindPin(static_cast<int>(inputPinId.Get()));
      Pin* outputPin = FindPin(static_cast<int>(outputPinId.Get()));

      if (inputPin && outputPin) {
        // 入力と出力が逆の場合は入れ替える
        if (inputPin->isInput == false && outputPin->isInput == true) {
          std::swap(inputPin, outputPin);
          std::swap(inputPinId, outputPinId);
        }

        // リンク作成の検証
        bool canCreateLink = inputPin->isInput != outputPin->isInput;

        if (canCreateLink && ed::AcceptNewItem()) {
          CreateLink(static_cast<int>(outputPinId.Get()), static_cast<int>(inputPinId.Get()));
        }
      }
    }

    // ノード作成（右クリックメニューなど）
    ed::PinId pinId;
    if (ed::QueryNewNode(&pinId)) {
      if (ed::AcceptNewItem()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 pos = ed::ScreenToCanvas(mousePos);
        int nodeId = CreateNode("New Node", pos.x, pos.y);

        // ピンが指定されていた場合は自動接続
        if (pinId.Get() != 0) {
          Pin* pin = FindPin(static_cast<int>(pinId.Get()));
          if (pin) {
            // 新しいノードに対応するピンを作成して接続
            int newPinId = AddPin(nodeId, "Auto", !pin->isInput);
            if (pin->isInput) {
              CreateLink(newPinId, static_cast<int>(pinId.Get()));
            }
            else {
              CreateLink(static_cast<int>(pinId.Get()), newPinId);
            }
          }
        }
      }
    }
  }
  ed::EndCreate();
}

void NodeEditorManager::HandleDeletion() {
  if (ed::BeginDelete()) {
    // リンクの削除
    ed::LinkId deletedLinkId;
    while (ed::QueryDeletedLink(&deletedLinkId)) {
      if (ed::AcceptDeletedItem()) {
        DeleteLink(static_cast<int>(deletedLinkId.Get()));
      }
    }

    // ノードの削除
    ed::NodeId deletedNodeId;
    while (ed::QueryDeletedNode(&deletedNodeId)) {
      if (ed::AcceptDeletedItem()) {
        DeleteNode(static_cast<int>(deletedNodeId.Get()));
      }
    }
  }
  ed::EndDelete();
}

int NodeEditorManager::CreateNode(const std::string& name, float x, float y) {
  Node newNode;
  newNode.id = nextNodeId_++;
  newNode.name = name;
  newNode.posX = x;
  newNode.posY = y;

  nodes_.push_back(newNode);

  // デフォルトピンを追加
  AddPin(newNode.id, "Input", true);
  AddPin(newNode.id, "Output", false);

  return newNode.id;
}

int NodeEditorManager::AddPin(int nodeId, const std::string& name, bool isInput, const std::string& type) {
  Node* node = FindNode(nodeId);
  if (!node) return -1;

  Pin newPin;
  newPin.id = nextPinId_++;
  newPin.nodeId = nodeId;
  newPin.name = name;
  newPin.isInput = isInput;
  newPin.type = type;

  pins_.push_back(newPin);

  if (isInput) {
    node->inputPins.push_back(newPin.id);
  }
  else {
    node->outputPins.push_back(newPin.id);
  }

  return newPin.id;
}

int NodeEditorManager::CreateLink(int startPinId, int endPinId) {
  // ピンの検証
  Pin* startPin = FindPin(startPinId);
  Pin* endPin = FindPin(endPinId);

  if (!startPin || !endPin) return -1;
  if (startPin->isInput == endPin->isInput) return -1; // 同じ種類のピン同士は接続不可

  // 既存のリンクをチェック
  for (const auto& link : links_) {
    if ((link.startPinId == startPinId && link.endPinId == endPinId) ||
      (link.startPinId == endPinId && link.endPinId == startPinId)) {
      return -1; // 既に接続されている
    }
  }

  Link newLink;
  newLink.id = nextLinkId_++;
  newLink.startPinId = startPinId;
  newLink.endPinId = endPinId;

  links_.push_back(newLink);

  return newLink.id;
}

void NodeEditorManager::DeleteNode(int nodeId) {
  // ノードに関連するリンクを削除
  links_.erase(
    std::remove_if(links_.begin(), links_.end(),
      [this, nodeId](const Link& link) {
        Pin* startPin = FindPin(link.startPinId);
        Pin* endPin = FindPin(link.endPinId);
        return (startPin && startPin->nodeId == nodeId) ||
          (endPin && endPin->nodeId == nodeId);
      }),
    links_.end()
  );

  // ノードのピンを削除
  pins_.erase(
    std::remove_if(pins_.begin(), pins_.end(),
      [nodeId](const Pin& pin) {
        return pin.nodeId == nodeId;
      }),
    pins_.end()
  );

  // ノード自体を削除
  nodes_.erase(
    std::remove_if(nodes_.begin(), nodes_.end(),
      [nodeId](const Node& node) {
        return node.id == nodeId;
      }),
    nodes_.end()
  );
}

void NodeEditorManager::DeleteLink(int linkId) {
  links_.erase(
    std::remove_if(links_.begin(), links_.end(),
      [linkId](const Link& link) {
        return link.id == linkId;
      }),
    links_.end()
  );
}

void NodeEditorManager::Clear() {
  nodes_.clear();
  pins_.clear();
  links_.clear();

  // ID範囲を分離して競合を防ぐ
  nextNodeId_ = 1000;  // ノード: 1000番台
  nextPinId_ = 2000;   // ピン: 2000番台
  nextLinkId_ = 3000;  // リンク: 3000番台
  firstFrame_ = true;  // リセット時にfirstFrameフラグも初期化
}

// ヘルパー関数の実装
NodeEditorManager::Node* NodeEditorManager::FindNode(int nodeId) {
  for (auto& node : nodes_) {
    if (node.id == nodeId) {
      return &node;
    }
  }
  return nullptr;
}

NodeEditorManager::Pin* NodeEditorManager::FindPin(int pinId) {
  for (auto& pin : pins_) {
    if (pin.id == pinId) {
      return &pin;
    }
  }
  return nullptr;
}

NodeEditorManager::Link* NodeEditorManager::FindLink(int linkId) {
  for (auto& link : links_) {
    if (link.id == linkId) {
      return &link;
    }
  }
  return nullptr;
}

void NodeEditorManager::CreateSampleGraph() {
  // サンプルグラフの作成
  int node1 = CreateNode("Start", 100, 100);
  int node2 = CreateNode("Process A", 300, 50);
  int node3 = CreateNode("Process B", 300, 200);
  int node4 = CreateNode("End", 500, 125);

  // 追加のピンを作成
  AddPin(node2, "Input2", true);
  AddPin(node3, "Output2", false);

  // ノード間のリンクを作成
  Node* startNode = FindNode(node1);
  Node* processANode = FindNode(node2);
  Node* processBNode = FindNode(node3);
  Node* endNode = FindNode(node4);

  if (startNode && processANode && processBNode && endNode) {
    // Start -> Process A
    CreateLink(startNode->outputPins[0], processANode->inputPins[0]);

    // Start -> Process B
    CreateLink(startNode->outputPins[0], processBNode->inputPins[0]);

    // Process A -> End
    CreateLink(processANode->outputPins[0], endNode->inputPins[0]);

    // Process B -> End
    CreateLink(processBNode->outputPins[0], endNode->inputPins[0]);
  }
}

void NodeEditorManager::AutoArrangeNodes() {
  // 簡単な自動整列アルゴリズム
  if (nodes_.empty()) return;

  const float horizontalSpacing = 200.0f;
  const float verticalSpacing = 150.0f;

  // レイヤーごとにノードを配置
  float currentX = 50.0f;
  float currentY = 50.0f;
  int nodesPerRow = static_cast<int>(std::sqrt(static_cast<float>(nodes_.size())));
  int currentNodeInRow = 0;

  for (auto& node : nodes_) {
    node.posX = currentX;
    node.posY = currentY;
    ed::SetNodePosition(node.id, ImVec2(node.posX, node.posY));

    currentNodeInRow++;
    if (currentNodeInRow >= nodesPerRow) {
      currentNodeInRow = 0;
      currentX = 50.0f;
      currentY += verticalSpacing;
    }
    else {
      currentX += horizontalSpacing;
    }
  }
}

#endif // _DEBUG