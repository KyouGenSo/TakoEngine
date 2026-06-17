#pragma once
#include "BTComposite.h"
#include "BTBlackboard.h"

namespace Tako {

  /// <summary>
  /// セレクターノード (OR ロジック)。
  /// 子ノードを順に実行し、最初の Success で停止する。全失敗なら Failure を返す。
  /// </summary>
  class BTSelector : public BTComposite {
  public:
    BTSelector();

    virtual ~BTSelector() = default;

    /// <summary>
    /// ノードの実行。子ノードを直列に順次評価する。
    /// </summary>
    /// <param name="blackboard">ブラックボード</param>
    /// <returns>実行結果 (いずれか Success → Success、全 Failure → Failure、Running 中の子があれば Running)</returns>
    BTNodeStatus Execute(BTBlackboard* blackboard) override;
  };

} // namespace Tako
