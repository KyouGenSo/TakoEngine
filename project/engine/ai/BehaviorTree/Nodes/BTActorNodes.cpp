#include "BTActorNodes.h"
#include "Actor.h"
#include "BTBlackboard.h"

#include <algorithm>
#include <cmath>

namespace Tako {

  namespace {

    /// <summary>
    /// ツリーを動かしている Actor と、ref が指す相手。どちらかが居なければ false
    /// </summary>
    bool FindActors(const BTBlackboard& blackboard, const ActorRef& ref, Actor*& owner, Actor*& target)
    {
      owner  = blackboard.GetPtr<Actor>(kBTOwnerKey);
      target = owner ? owner->Resolve(ref) : nullptr;
      return target != nullptr;
    }

  } // namespace

  BTNodeStatus BTWait::Execute(BTBlackboard* blackboard)
  {
    elapsed_ += blackboard->GetDeltaTime();
    if (elapsed_ < seconds_) {
      status_ = BTNodeStatus::Running;
      return status_;
    }
    elapsed_ = 0.0f;
    status_  = BTNodeStatus::Success;
    return status_;
  }

  void BTWait::Reset()
  {
    BTNode::Reset();
    elapsed_ = 0.0f;
  }

  BTNodeStatus BTMoveToActor::Execute(BTBlackboard* blackboard)
  {
    Actor* owner  = nullptr;
    Actor* target = nullptr;
    if (!FindActors(*blackboard, target_, owner, target)) {
      status_ = BTNodeStatus::Failure;
      return status_;
    }
    const Vector3 position = owner->GetWorldPosition();
    const Vector3 toTarget = target->GetWorldPosition() - position;
    const float   distance = toTarget.Length();
    if (distance <= stopDistance_) {
      status_ = BTNodeStatus::Success;
      return status_;
    }
    const float step = (std::min)(speed_ * blackboard->GetDeltaTime(), distance - stopDistance_);
    owner->SetWorldPosition(position + toTarget * (step / distance));
    if (facesTarget_) {
      // +Z が前
      owner->GetTransform().rotate.y = std::atan2(toTarget.x, toTarget.z);
    }
    status_ = BTNodeStatus::Running;
    return status_;
  }

  BTNodeStatus BTActorDistanceCondition::Execute(BTBlackboard* blackboard)
  {
    Actor* owner  = nullptr;
    Actor* target = nullptr;
    if (!FindActors(*blackboard, target_, owner, target)) {
      status_ = BTNodeStatus::Failure;
      return status_;
    }
    const float distance = (target->GetWorldPosition() - owner->GetWorldPosition()).Length();
    status_ = distance >= minDistance_ && distance <= maxDistance_ ? BTNodeStatus::Success : BTNodeStatus::Failure;
    return status_;
  }

} // namespace Tako
