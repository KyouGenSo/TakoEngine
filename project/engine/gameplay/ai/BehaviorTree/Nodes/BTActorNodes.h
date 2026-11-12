#pragma once
#include "ActorRef.h"
#include "BTNode.h"

namespace Tako {

  // BehaviorTreeComponent が Blackboard に入れる、ツリーを動かしている Actor（Actor*）のキー
  inline constexpr const char* kBTOwnerKey = "owner";

  /// <summary>
  /// 指定した秒数だけ Running を返し、その後 Success を返す
  /// </summary>
  TCLASS(DisplayName = "Wait", Category = "Action", Color = 0x8C8C8C)
  class TAKO_API BTWait : public BTNode {
    GENERATED_BODY()

  public: //メンバー関数
    BTNodeStatus Execute(BTBlackboard* blackboard) override;

    void Reset() override;

  private: //メンバー変数
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float seconds_ = 1.0f;

    float elapsed_ = 0.0f;
  };

  /// <summary>
  /// ツリーを動かしている Actor を、target の Actor へ一定の速さで近づける。止まる距離まで来たら Success
  /// </summary>
  TCLASS(DisplayName = "Move To Actor", Category = "Action", Color = 0x4DB380)
  class TAKO_API BTMoveToActor : public BTNode {
    GENERATED_BODY()

  public: //メンバー関数
    /// <returns>相手が居なければ Failure、近づいている間は Running</returns>
    BTNodeStatus Execute(BTBlackboard* blackboard) override;

  private: //メンバー変数
    TPROPERTY(Edit)
    ActorRef target_;
    TPROPERTY(Edit, Tooltip = "1 秒あたりの移動量", ClampMin = 0, Speed = 0.05)
    float speed_ = 3.0f;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float stopDistance_ = 1.5f;
    TPROPERTY(Edit, Tooltip = "進む向きへ Y 軸で回す")
    bool facesTarget_ = true;
  };

  /// <summary>
  /// ツリーを動かしている Actor と target の Actor の距離が、最小と最大の間なら Success
  /// </summary>
  TCLASS(DisplayName = "Actor Distance", Category = "Condition", Color = 0x33B2B2)
  class TAKO_API BTActorDistanceCondition : public BTNode {
    GENERATED_BODY()

  public: //メンバー関数
    BTNodeStatus Execute(BTBlackboard* blackboard) override;

  private: //メンバー変数
    TPROPERTY(Edit)
    ActorRef target_;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float minDistance_ = 0.0f;
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.05)
    float maxDistance_ = 5.0f;
  };

} // namespace Tako
