#include "BTSelector.h"

namespace Tako {

  BTSelector::BTSelector() {
    name_ = "Selector";
  }

  BTNodeStatus BTSelector::Execute(BTBlackboard* blackboard) {
    if (children_.empty()) {
      return BTNodeStatus::Failure;
    }

    // 前回 Running だった場合、その子ノードから続行
    for (size_t i = currentChildIndex_; i < children_.size(); ++i) {
      BTNodeStatus childStatus = children_[i]->Execute(blackboard);

      if (childStatus == BTNodeStatus::Success) {
        // 成功したら即座に成功を返す
        currentChildIndex_ = 0;
        status_ = BTNodeStatus::Success;
        return status_;
      }
      else if (childStatus == BTNodeStatus::Running) {
        // 実行中なら現在のインデックスを記憶
        currentChildIndex_ = i;
        status_ = BTNodeStatus::Running;
        return status_;
      }
      // Failure の場合は次の子ノードへ
    }

    // 全ての子ノードが失敗
    currentChildIndex_ = 0;
    status_ = BTNodeStatus::Failure;
    return status_;
  }

} // namespace Tako
