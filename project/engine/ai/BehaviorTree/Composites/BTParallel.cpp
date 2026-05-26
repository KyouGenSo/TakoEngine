#include "BTParallel.h"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace Tako {

  namespace {
    // policy 文字列 → Policy 列挙値
    BTParallel::Policy PolicyFromString(const std::string& s) {
      if (s == "AllSuccess") return BTParallel::Policy::AllSuccess;
      if (s == "AnySuccess") return BTParallel::Policy::AnySuccess;
      if (s == "MainChild")  return BTParallel::Policy::MainChild;
      // 未知文字列は MainChild にフォールバック (最も安全)
      return BTParallel::Policy::MainChild;
    }

    // Policy 列挙値 → 文字列
    const char* PolicyToString(BTParallel::Policy p) {
      switch (p) {
      case BTParallel::Policy::AllSuccess: return "AllSuccess";
      case BTParallel::Policy::AnySuccess: return "AnySuccess";
      case BTParallel::Policy::MainChild:  return "MainChild";
      }
      return "MainChild";
    }
  } // namespace

  BTParallel::BTParallel(Policy policy)
    : policy_(policy)
  {
    name_ = "Parallel";
  }

  BTNodeStatus BTParallel::Execute(BTBlackboard* blackboard) {
    if (children_.empty()) {
      // 子なしは Success
      status_ = BTNodeStatus::Success;
      return status_;
    }

    // 子ステータス配列のサイズ調整 (初回・子変更時のリサイズ)。
    // 既に対応サイズなら状態は保持される (途中フレームの判定継続のため)。
    if (childStatuses_.size() != children_.size()) {
      childStatuses_.assign(children_.size(), BTNodeStatus::Running);
    }

    // 全 Running 子を 1 フレームで並列 tick
    for (size_t i = 0; i < children_.size(); ++i) {
      if (childStatuses_[i] == BTNodeStatus::Running) {
        childStatuses_[i] = children_[i]->Execute(blackboard);
      }
    }

    // ポリシーに応じた終了判定
    auto resetRunningChildren = [this](size_t skipIndex) {
      for (size_t i = 0; i < children_.size(); ++i) {
        if (i == skipIndex) continue;
        if (childStatuses_[i] == BTNodeStatus::Running) {
          children_[i]->Reset();
        }
      }
      };
    constexpr size_t kSkipNone = static_cast<size_t>(-1);

    switch (policy_) {
    case Policy::AllSuccess: {
      bool anyRunning = false;
      bool anyFailure = false;
      for (auto s : childStatuses_) {
        if (s == BTNodeStatus::Running) anyRunning = true;
        if (s == BTNodeStatus::Failure) anyFailure = true;
      }
      if (anyFailure) {
        // 1 つでも失敗 → 残り Running を中断して Failure
        resetRunningChildren(kSkipNone);
        childStatuses_.clear();
        status_ = BTNodeStatus::Failure;
        return status_;
      }
      if (anyRunning) {
        status_ = BTNodeStatus::Running;
        return status_;
      }
      // 全 Success
      childStatuses_.clear();
      status_ = BTNodeStatus::Success;
      return status_;
    }

    case Policy::AnySuccess: {
      // 1 つでも Success があれば即終了
      for (auto s : childStatuses_) {
        if (s == BTNodeStatus::Success) {
          resetRunningChildren(kSkipNone);
          childStatuses_.clear();
          status_ = BTNodeStatus::Success;
          return status_;
        }
      }
      bool anyRunning = false;
      for (auto s : childStatuses_) {
        if (s == BTNodeStatus::Running) anyRunning = true;
      }
      if (anyRunning) {
        status_ = BTNodeStatus::Running;
        return status_;
      }
      // 全 Failure
      childStatuses_.clear();
      status_ = BTNodeStatus::Failure;
      return status_;
    }

    case Policy::MainChild: {
      // 子[0] が Running 以外になった瞬間に他を中断し、子[0] の結果を返す
      if (childStatuses_[0] != BTNodeStatus::Running) {
        resetRunningChildren(0);
        const BTNodeStatus mainResult = childStatuses_[0];
        childStatuses_.clear();
        status_ = mainResult;
        return status_;
      }
      status_ = BTNodeStatus::Running;
      return status_;
    }
    }

    // 列挙値の網羅外 (到達不能だが安全弁)
    status_ = BTNodeStatus::Success;
    return status_;
  }

  void BTParallel::Reset() {
    BTComposite::Reset();    // 全子 Reset + currentChildIndex_ = 0
    childStatuses_.clear();  // 並列状態キャッシュもクリア
  }

  void BTParallel::ApplyParameters(const nlohmann::json& params) {
    if (params.contains("policy") && params["policy"].is_string()) {
      policy_ = PolicyFromString(params["policy"].get<std::string>());
    }
  }

  nlohmann::json BTParallel::ExtractParameters() const {
    return nlohmann::json{
        { "policy", PolicyToString(policy_) }
    };
  }

#ifdef _DEBUG
  bool BTParallel::DrawImGui() {
    bool changed = false;
    const char* items[] = { "AllSuccess", "AnySuccess", "MainChild" };
    int currentIndex = static_cast<int>(policy_);
    if (ImGui::Combo("Policy", &currentIndex, items, IM_ARRAYSIZE(items))) {
      policy_ = static_cast<Policy>(currentIndex);
      changed = true;
    }
    return changed;
  }
#endif

} // namespace Tako
