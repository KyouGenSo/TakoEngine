#pragma once
#include "BTComposite.h"
#include "BTBlackboard.h"
#include <vector>
#include <optional>

namespace Tako {

  /// <summary>
  /// ランダムセレクターノード。
  /// 子ノードをランダムな順序で実行し、最初の Success で停止する。
  /// 連続選択防止のため、前回成功した子が先頭に来た場合は他要素とスワップする。
  /// </summary>
  class BTRandomSelector : public BTComposite {
  public:
    /// <summary>
    /// コンストラクタ。ノード名を "RandomSelector" に設定する。
    /// </summary>
    BTRandomSelector();

    /// <summary>
    /// 仮想デストラクタ。
    /// </summary>
    virtual ~BTRandomSelector() = default;

    /// <summary>
    /// ノードの実行。シャッフル順で子ノードを評価する。
    /// </summary>
    /// <param name="blackboard">ブラックボード</param>
    /// <returns>実行結果</returns>
    BTNodeStatus Execute(BTBlackboard* blackboard) override;

    /// <summary>
    /// 状態のリセット。シャッフル必要フラグを立て直す。
    /// </summary>
    void Reset() override;

  private:
    /// <summary>
    /// 子ノードのインデックスをシャッフル。Fisher-Yates アルゴリズム使用。
    /// </summary>
    void ShuffleIndices();

  private:
    // シャッフルされたインデックス列
    std::vector<size_t> shuffledIndices_;

    // 現在のシャッフル済みインデックス位置 (Running 状態の継続用)
    size_t currentShuffledIdx_ = 0;

    // シャッフルが必要かどうか (新しい選択サイクル開始時に true)
    bool needsShuffle_ = true;

    // 前回成功した子ノードのインデックス (連続選択防止用)
    std::optional<size_t> lastSuccessIdx_;
  };

} // namespace Tako
