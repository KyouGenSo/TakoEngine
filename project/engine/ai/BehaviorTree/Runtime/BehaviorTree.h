#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <json.hpp>
#include "BTNode.h"
#include "BTBlackboard.h"

namespace Tako {

  /// <summary>
  /// ビヘイビアツリー実行ラッパ。
  /// ルートノードとブラックボードを保持し、Tick() で 1 フレーム実行する。
  /// JSON ロード/セーブは現状プレースホルダ。実際の JSON IO は BehaviorTreeEditor が担当する想定。
  /// </summary>
  class BehaviorTree {
  public:
    /// <summary>
    /// コンストラクタ。内部ブラックボードを生成する。
    /// </summary>
    BehaviorTree();

    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~BehaviorTree() = default;

    /// <summary>
    /// ルートノードの設定。
    /// </summary>
    /// <param name="root">設定するルートノード</param>
    void SetRootNode(BTNodePtr root) { root_ = root; }

    /// <summary>
    /// ルートノードの取得。
    /// </summary>
    /// <returns>現在のルートノード (未設定なら nullptr)</returns>
    BTNodePtr GetRootNode() const { return root_; }

    /// <summary>
    /// ブラックボード取得 (書き込み可)。
    /// </summary>
    /// <returns>内部ブラックボードへのポインタ</returns>
    BTBlackboard* GetBlackboard() { return blackboard_.get(); }

    /// <summary>
    /// ブラックボード取得 (読み取り専用)。
    /// </summary>
    /// <returns>内部ブラックボードへの const ポインタ</returns>
    const BTBlackboard* GetBlackboard() const { return blackboard_.get(); }

    /// <summary>
    /// 1 フレーム実行。deltaTime をブラックボードに伝えてルートノードを Execute する。
    /// </summary>
    /// <param name="deltaTime">フレーム経過時間 [秒]</param>
    void Tick(float deltaTime);

    /// <summary>
    /// ツリー全体のリセット。ルート以下の全ノードを Reset する。
    /// </summary>
    void Reset();

    /// <summary>
    /// 最後の Tick 実行結果を取得。
    /// </summary>
    /// <returns>最後の実行ステータス</returns>
    BTNodeStatus GetLastStatus() const { return lastStatus_; }

    /// <summary>
    /// 現在実行中のノードを取得 (デバッグ表示・エディタハイライト用)。
    /// </summary>
    /// <returns>Running 状態の最深ノード、なければ nullptr</returns>
    BTNodePtr GetCurrentRunningNode() const;

    /// <summary>
    /// JSON ファイルからツリーを読み込む。
    /// (現状はプレースホルダ。BehaviorTreeEditor 側で JSON IO を実装している)
    /// </summary>
    /// <param name="filepath">読み込み元 JSON ファイルパス</param>
    /// <returns>成功すれば true</returns>
    bool LoadFromJSON(const std::string& filepath);

  private:
    /// <summary>
    /// ノードを再帰的に探索し、Running 状態の最深ノードを返す。
    /// </summary>
    /// <param name="node">探索起点のノード</param>
    /// <returns>Running 状態の最深ノード、なければ nullptr</returns>
    BTNodePtr FindRunningNodeRecursive(const BTNodePtr& node) const;

    /// <summary>
    /// JSON のノード定義からツリーを再帰的に構築。
    /// BTNodeRegistry::Create でノード生成、リンク情報で子ノードを接続する。
    /// </summary>
    BTNodePtr BuildNodeFromJSON(
      const nlohmann::json& nodeJson,
      const std::unordered_map<int, nlohmann::json>& nodeMap,
      const std::vector<nlohmann::json>& links,
      std::unordered_set<int>& visitedNodes);

  private:
    /// ルートノード (ツリーのエントリポイント)
    BTNodePtr root_;

    /// 内部ブラックボード (所有)
    std::unique_ptr<BTBlackboard> blackboard_;

    /// 最後の Tick 実行結果
    BTNodeStatus lastStatus_ = BTNodeStatus::Failure;
  };

} // namespace Tako
