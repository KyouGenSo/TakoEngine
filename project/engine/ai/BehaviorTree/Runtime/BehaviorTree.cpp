#include "BehaviorTree.h"
#include "BTComposite.h"
#include "BTNodeRegistry.h"
#include <fstream>

namespace Tako {

  BehaviorTree::BehaviorTree()
    : blackboard_(std::make_unique<BTBlackboard>())
  {
  }

  void BehaviorTree::Tick(float deltaTime) {
    if (!root_) {
      lastStatus_ = BTNodeStatus::Failure;
      return;
    }
    blackboard_->SetDeltaTime(deltaTime);
    lastStatus_ = root_->Execute(blackboard_.get());
    // 完了 (Success/Failure) したら次フレームから再実行できるようリセット
    if (lastStatus_ != BTNodeStatus::Running) {
      root_->Reset();
    }
  }

  void BehaviorTree::Reset() {
    if (root_) {
      root_->Reset();
    }
    lastStatus_ = BTNodeStatus::Failure;
  }

  BTNodePtr BehaviorTree::GetCurrentRunningNode() const {
    if (!root_) {
      return nullptr;
    }
    return FindRunningNodeRecursive(root_);
  }

  BTNodePtr BehaviorTree::FindRunningNodeRecursive(const BTNodePtr& node) const {
    if (!node) {
      return nullptr;
    }
    if (node->GetStatus() != BTNodeStatus::Running) {
      return nullptr;
    }
    // コンポジットなら子の中の Running ノードを優先返却 (最深 Running が欲しい)
    if (node->IsComposite()) {
      auto* composite = dynamic_cast<BTComposite*>(node.get());
      if (composite) {
        for (const auto& child : composite->GetChildren()) {
          BTNodePtr found = FindRunningNodeRecursive(child);
          if (found) {
            return found;
          }
        }
      }
    }
    // 子に Running がなければ自身が最深 Running ノード
    return node;
  }

  bool BehaviorTree::LoadFromJSON(const std::string& filepath) {
    try {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        return false;
      }
      nlohmann::json json;
      file >> json;
      file.close();

      if (!json.contains("version") || json["version"] != "1.0") {
        return false;
      }

      // ノードマップ作成 (ID → JSON)
      std::unordered_map<int, nlohmann::json> nodeMap;
      if (json.contains("nodes")) {
        for (const auto& nodeJson : json["nodes"]) {
          nodeMap[nodeJson["id"].get<int>()] = nodeJson;
        }
      }

      std::vector<nlohmann::json> links;
      if (json.contains("links")) {
        links = json["links"].get<std::vector<nlohmann::json>>();
      }

      // ルートノード探索: 親リンクを持たないノード
      int rootNodeId = -1;
      std::unordered_set<int> childNodeIds;
      for (const auto& link : links) {
        childNodeIds.insert(link["targetNodeId"].get<int>());
      }
      for (const auto& [nodeId, nodeJson] : nodeMap) {
        if (childNodeIds.find(nodeId) == childNodeIds.end()) {
          rootNodeId = nodeId;
          break;
        }
      }
      // フォールバック: 最初のノードをルートに
      if (rootNodeId == -1 && !nodeMap.empty()) {
        rootNodeId = nodeMap.begin()->first;
      }
      if (rootNodeId == -1) {
        return false;
      }

      std::unordered_set<int> visitedNodes;
      root_ = BuildNodeFromJSON(nodeMap[rootNodeId], nodeMap, links, visitedNodes);
      if (!root_) {
        return false;
      }

      Reset();
      return true;
    }
    catch (const std::exception&) {
      return false;
    }
  }

  BTNodePtr BehaviorTree::BuildNodeFromJSON(
    const nlohmann::json& nodeJson,
    const std::unordered_map<int, nlohmann::json>& nodeMap,
    const std::vector<nlohmann::json>& links,
    std::unordered_set<int>& visitedNodes) {

    if (nodeJson.empty()) return nullptr;

    int nodeId = nodeJson["id"];
    // 循環参照ガード
    if (visitedNodes.find(nodeId) != visitedNodes.end()) {
      return nullptr;
    }
    visitedNodes.insert(nodeId);

    std::string nodeType = nodeJson["type"];
    BTNodePtr node = BTNodeRegistry::GetInstance()->Create(nodeType);
    if (!node) {
      return nullptr;
    }

    if (nodeJson.contains("parameters") && !nodeJson["parameters"].is_null()) {
      node->ApplyParameters(nodeJson["parameters"]);
    }
    if (nodeJson.contains("displayName")) {
      node->SetName(nodeJson["displayName"]);
    }

    // コンポジットなら子ノードを再帰構築して接続
    if (node->IsComposite()) {
      auto compositeNode = std::dynamic_pointer_cast<BTComposite>(node);
      if (compositeNode) {
        std::vector<int> childIds;
        for (const auto& link : links) {
          if (link["sourceNodeId"] == nodeId) {
            childIds.push_back(link["targetNodeId"]);
          }
        }
        for (int childId : childIds) {
          auto childIt = nodeMap.find(childId);
          if (childIt != nodeMap.end()) {
            BTNodePtr childNode = BuildNodeFromJSON(childIt->second, nodeMap, links, visitedNodes);
            if (childNode) {
              compositeNode->AddChild(childNode);
            }
          }
        }
      }
    }

    return node;
  }

} // namespace Tako
