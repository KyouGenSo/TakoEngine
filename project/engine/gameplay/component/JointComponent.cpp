#include "JointComponent.h"
#include "Actor.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "RigidbodyComponent.h"

namespace Tako {

  JointComponent::JointComponent() = default;

  JointComponent::~JointComponent()
  {
    EndPlay();
  }

  void JointComponent::BeginPlay()
  {
    isPlaying_ = true;
    CreateJoint();
  }

  void JointComponent::EndPlay()
  {
    isPlaying_ = false;
    DestroyJoint();
  }

  void JointComponent::PostEditChange()
  {
    if (isPlaying_) {
      DestroyJoint();
      CreateJoint();
    }
  }

  void JointComponent::SetType(JointType type)
  {
    type_ = type;
    PostEditChange();
  }

  void JointComponent::SetConnectedActor(const Actor* actor)
  {
    connectedBody_ = actor ? ActorRef{ actor->GetId() } : ActorRef{};
    PostEditChange();
  }

  void JointComponent::SetAnchor(const Vector3& anchor)
  {
    anchor_ = anchor;
    PostEditChange();
  }

  void JointComponent::SetConnectedAnchor(const Vector3& anchor)
  {
    connectedAnchor_ = anchor;
    PostEditChange();
  }

  void JointComponent::SetAxis(const Vector3& axis)
  {
    axis_ = axis;
    PostEditChange();
  }

  void JointComponent::SetAngleLimits(float minAngle, float maxAngle)
  {
    minAngle_ = minAngle;
    maxAngle_ = maxAngle;
    PostEditChange();
  }

  void JointComponent::SetDistanceLimits(float minDistance, float maxDistance)
  {
    minDistance_ = minDistance;
    maxDistance_ = maxDistance;
    PostEditChange();
  }

  void JointComponent::CreateJoint()
  {
    Actor*                    owner     = GetOwner();
    const RigidbodyComponent* rigidbody = owner->GetComponent<RigidbodyComponent>();
    if (!rigidbody) {
      Logger::Log("Joint: %s has no Rigidbody, so the joint was not made", owner->GetName().c_str());
      return;
    }

    PhysicsWorld::JointSettings settings;
    switch (type_) {
    case JointType::Fixed:    settings.type = PhysicsWorld::JointType::Fixed; break;
    case JointType::Point:    settings.type = PhysicsWorld::JointType::Point; break;
    case JointType::Hinge:    settings.type = PhysicsWorld::JointType::Hinge; break;
    case JointType::Distance: settings.type = PhysicsWorld::JointType::Distance; break;
    }
    // つなぐ点と軸は、今の Actor の位置と向きでワールドに直す
    const Matrix4x4 world = owner->GetWorldMatrix();
    settings.body            = rigidbody->GetBody();
    settings.anchor          = Mat4x4::Transform(world, anchor_);
    settings.axis            = Mat4x4::TransformNormal(world, axis_);
    settings.connectedAnchor = connectedAnchor_;
    settings.minAngle        = minAngle_;
    settings.maxAngle        = maxAngle_;
    settings.minDistance     = minDistance_;
    settings.maxDistance     = maxDistance_;

    if (const Actor* connected = owner->Resolve(connectedBody_)) {
      const RigidbodyComponent* connectedRigidbody = connected->GetComponent<RigidbodyComponent>();
      if (!connectedRigidbody) {
        Logger::Log("Joint: %s is connected to %s, which has no Rigidbody, so the joint was not made", owner->GetName().c_str(), connected->GetName().c_str());
        return;
      }
      settings.connectedBody   = connectedRigidbody->GetBody();
      settings.connectedAnchor = Mat4x4::Transform(connected->GetWorldMatrix(), connectedAnchor_);
    }
    joint_ = PhysicsWorld::GetInstance()->CreateJoint(settings);
  }

  void JointComponent::DestroyJoint()
  {
    if (joint_ != PhysicsWorld::kInvalidJoint) {
      PhysicsWorld::GetInstance()->DestroyJoint(joint_);
      joint_ = PhysicsWorld::kInvalidJoint;
    }
  }

} // namespace Tako
