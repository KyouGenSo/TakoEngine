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
        currentChildIndex_ = 0;
        status_ = BTNodeStatus::Success;
        return status_;
      }
      else if (childStatus == BTNodeStatus::Running) {
        currentChildIndex_ = i;
        status_ = BTNodeStatus::Running;
        return status_;
      }
    }

    currentChildIndex_ = 0;
    status_ = BTNodeStatus::Failure;
    return status_;
  }

} // namespace Tako
