#pragma once
#include "BTNode.h"
#include <vector>

namespace Tako {

  /// <summary>
  /// コンポジットノードの基底クラス。
  /// 複数の子ノードを持つノード (Sequence / Selector / Parallel / RandomSelector など)。
  /// </summary>
  class BTComposite : public BTNode {
  public: //メンバー関数
    BTComposite() = default;

    virtual ~BTComposite() = default;

    /// <summary>
    /// 子ノードの追加 (末尾に push_back)。nullptr は無視する。
    /// </summary>
    /// <param name="child">追加する子ノード</param>
    void AddChild(BTNodePtr child);

    /// <summary>
    /// 子ノードの削除 (一致する全要素を消す)。
    /// </summary>
    /// <param name="child">削除する子ノード</param>
    void RemoveChild(BTNodePtr child);

    void ClearChildren();

    /// <summary>
    /// ノードのリセット (自身 + 全子ノードを再帰的にリセット)。
    /// </summary>
    void Reset() override;

    //===============================================
    //Getter
    //===============================================
    const std::vector<BTNodePtr>& GetChildren() const { return children_; }

    size_t GetChildCount() const { return children_.size(); }

    bool IsComposite() const override { return true; }

  protected: //メンバー変数
    std::vector<BTNodePtr> children_;               ///< 子ノード
    size_t                 currentChildIndex_ = 0;  ///< 現在実行中の子ノードのインデックス (Sequence/Selector の Running 継続用)
  };

} // namespace Tako
