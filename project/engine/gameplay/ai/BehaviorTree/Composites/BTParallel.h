#pragma once
#include "BTComposite.h"
#include "BTBlackboard.h"

namespace Tako {

  /// <summary>
  /// パラレルノード (並列実行コンポジット)。
  /// 全ての子ノードを 1 フレームで並列に Execute し、
  /// 終了判定は Policy に応じて切り替える。
  /// </summary>
  TCLASS(DisplayName = "Parallel", Category = "Composite", Color = 0x66B366)
  class TAKO_API BTParallel : public BTComposite {
    GENERATED_BODY()

  public: //構造体
    /// <summary>
    /// 終了判定ポリシー。保存は列挙子の名前
    /// </summary>
    TENUM()
    enum class Policy : uint32_t {
      /// <summary>
      /// 全ての子が Success → Success。
      /// 1 つでも Failure になった瞬間 → 残り Running 子を Reset し Failure。
      /// </summary>
      AllSuccess = 0,

      /// <summary>
      /// 1 つでも Success → 残り Running 子を Reset し Success。
      /// 全 Failure → Failure。
      /// </summary>
      AnySuccess = 1,

      /// <summary>
      /// 子[0] が Running 以外になった瞬間 → 残り Running 子を Reset し、子[0] の結果を返す。
      /// (メイン攻撃 + サブ攻撃の並列実行用。メインが終わればサブも終わる)
      /// </summary>
      MainChild = 2
    };

  public: //メンバー関数
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    /// <param name="policy">終了判定ポリシー (デフォルト: MainChild)</param>
    explicit BTParallel(Policy policy = Policy::MainChild);

    virtual ~BTParallel() = default;

    /// <summary>
    /// ノードの実行。全子を 1 フレームで並列に tick し、ポリシーに従って判定する。
    /// </summary>
    /// <param name="blackboard">ブラックボード</param>
    /// <returns>実行結果</returns>
    BTNodeStatus Execute(BTBlackboard* blackboard) override;

    /// <summary>
    /// ノードのリセット。並列状態キャッシュもクリアする。
    /// </summary>
    void Reset() override;

    //===========================
    //Setter
    //===========================
    void SetPolicy(Policy policy) { policy_ = policy; }

    //===========================
    //Getter
    //===========================
    Policy GetPolicy() const { return policy_; }

  private: //メンバー変数
    TPROPERTY(Edit)
    Policy                    policy_;         ///< ポリシー
    std::vector<BTNodeStatus> childStatuses_;  ///< 各子ノードの最新ステータス (初回 Execute 時に children_.size() に合わせて初期化)。一度 Success/Failure になった子は再 Execute されずに状態を保持する (ポリシー判定用)
  };

} // namespace Tako
