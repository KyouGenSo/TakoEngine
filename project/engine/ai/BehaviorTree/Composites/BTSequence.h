#pragma once
#include "BTComposite.h"
#include "BTBlackboard.h"

namespace Tako {

  /// <summary>
  /// シーケンスノード (AND ロジック)。
  /// 子ノードを順に実行し、最初の Failure で停止する。全成功なら Success を返す。
  /// </summary>
  class BTSequence : public BTComposite {
  public:
    BTSequence();

    virtual ~BTSequence() = default;

    /// <summary>
    /// ノードの実行。子ノードを直列に順次評価する。
    /// </summary>
    /// <param name="blackboard">ブラックボード</param>
    /// <returns>実行結果 (全 Success → Success、いずれか Failure → Failure、Running 中の子があれば Running)</returns>
    BTNodeStatus Execute(BTBlackboard* blackboard) override;
  };

} // namespace Tako
