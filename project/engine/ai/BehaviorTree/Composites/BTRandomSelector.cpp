#include "BTRandomSelector.h"
#include "RandomEngine.h"
#include <algorithm>

namespace Tako {

  BTRandomSelector::BTRandomSelector() {
    name_ = "RandomSelector";
  }

  BTNodeStatus BTRandomSelector::Execute(BTBlackboard* blackboard) {
    if (children_.empty()) {
      return BTNodeStatus::Failure;
    }

    // 新しい選択サイクルの開始時のみシャッフル
    if (needsShuffle_) {
      ShuffleIndices();
      needsShuffle_ = false;
      currentShuffledIdx_ = 0;
    }

    // シャッフル順で実行 (前回の Running 位置から継続)
    for (size_t i = currentShuffledIdx_; i < shuffledIndices_.size(); ++i) {
      size_t idx = shuffledIndices_[i];
      BTNodeStatus childStatus = children_[idx]->Execute(blackboard);

      if (childStatus == BTNodeStatus::Success) {
        lastSuccessIdx_ = idx;  // 前回成功した子を記録 (連続選択防止用)
        needsShuffle_ = true;   // 次回はシャッフル
        status_ = BTNodeStatus::Success;
        return status_;
      }
      else if (childStatus == BTNodeStatus::Running) {
        currentShuffledIdx_ = i;  // 現在位置を記憶 (シャッフルしない)
        status_ = BTNodeStatus::Running;
        return status_;
      }
      // Failure の場合は次へ
    }

    // 全て失敗
    needsShuffle_ = true;  // 次回はシャッフル
    status_ = BTNodeStatus::Failure;
    return status_;
  }

  void BTRandomSelector::Reset() {
    BTComposite::Reset();  // 子ノードへのリセット伝播を含む
    currentShuffledIdx_ = 0;
    needsShuffle_ = true;
  }

  void BTRandomSelector::ShuffleIndices() {
    shuffledIndices_.resize(children_.size());
    for (size_t i = 0; i < children_.size(); ++i) {
      shuffledIndices_[i] = i;
    }

    // Fisher-Yates シャッフル (RandomEngine 使用)
    RandomEngine* rng = RandomEngine::GetInstance();
    for (size_t i = shuffledIndices_.size() - 1; i > 0; --i) {
      size_t j = static_cast<size_t>(rng->GetInt(0, static_cast<int>(i)));
      std::swap(shuffledIndices_[i], shuffledIndices_[j]);
    }

    // 前回成功した子が先頭に来た場合、別の位置にスワップして連続選択を防止
    if (lastSuccessIdx_.has_value() && shuffledIndices_.size() > 1) {
      if (shuffledIndices_[0] == lastSuccessIdx_.value()) {
        size_t swapPos = static_cast<size_t>(
          rng->GetInt(1, static_cast<int>(shuffledIndices_.size() - 1)));
        std::swap(shuffledIndices_[0], shuffledIndices_[swapPos]);
      }
    }
  }

} // namespace Tako
