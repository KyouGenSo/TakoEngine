#pragma once
#include "BTNode.h"
#include <vector>

namespace Tako {

  /// <summary>
  /// コンポジットノードの基底クラス。
  /// 複数の子ノードを持つノード (Sequence / Selector / Parallel / RandomSelector など)。
  /// </summary>
  class BTComposite : public BTNode {
  public:
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    BTComposite() = default;

    /// <summary>
    /// 仮想デストラクタ。
    /// </summary>
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

    /// <summary>
    /// 全子ノードのクリア。
    /// </summary>
    void ClearChildren();

    /// <summary>
    /// 子ノードのリスト取得。
    /// </summary>
    /// <returns>子ノードのリスト (const 参照)</returns>
    const std::vector<BTNodePtr>& GetChildren() const { return children_; }

    /// <summary>
    /// 子ノードの個数取得。
    /// </summary>
    /// <returns>子ノードの個数</returns>
    size_t GetChildCount() const { return children_.size(); }

    /// <summary>
    /// コンポジットノードかどうか。BTComposite なので常に true。
    /// </summary>
    /// <returns>常に true</returns>
    bool IsComposite() const override { return true; }

    /// <summary>
    /// ノードのリセット (自身 + 全子ノードを再帰的にリセット)。
    /// </summary>
    void Reset() override;

  protected:
    /// 子ノードのリスト
    std::vector<BTNodePtr> children_;

    /// 現在実行中の子ノードのインデックス (Sequence/Selector の Running 継続用)
    size_t currentChildIndex_ = 0;
  };

} // namespace Tako
