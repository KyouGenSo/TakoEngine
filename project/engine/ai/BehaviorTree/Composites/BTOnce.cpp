#include "BTOnce.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace Tako {

  BTOnce::BTOnce() {
    name_ = "Once";
  }

  BTNodeStatus BTOnce::Execute(BTBlackboard* blackboard) {
    // 上限到達 or 子なしは Failure を返し、親 Selector を次へ進ませる
    if (usedCount_ >= maxUses_ || children_.empty()) {
      status_ = BTNodeStatus::Failure;
      return status_;
    }

    // 先頭の子のみ対象 (複数子は 2 つ目以降を無視)
    BTNodeStatus childStatus = children_[0]->Execute(blackboard);

    // 完了 (Success) 時のみ消費。中断 (Failure / Running) では消費しない
    if (childStatus == BTNodeStatus::Success) {
      ++usedCount_;
    }
    status_ = childStatus;
    return status_;
  }

  void BTOnce::ApplyParameters(const nlohmann::json& params) {
    if (params.contains("maxUses") && params["maxUses"].is_number_integer()) {
      maxUses_ = params["maxUses"].get<int>();
    }
  }

  nlohmann::json BTOnce::ExtractParameters() const {
    return nlohmann::json{
        { "maxUses", maxUses_ }
    };
  }

#ifdef _DEBUG
  bool BTOnce::DrawImGui() {
    bool changed = false;
    if (ImGui::InputInt("MaxUses", &maxUses_)) {
      if (maxUses_ < 1) maxUses_ = 1;
      changed = true;
    }
    return changed;
  }
#endif

} // namespace Tako
