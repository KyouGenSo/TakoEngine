#include "RigidbodyComponent.h"
#include "Actor.h"
#include "ColliderComponent.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "QuatFunc.h"

#include <utility>

#ifdef TAKO_EDITOR
#include "EditorUI.h"
#include "imgui.h"
#endif

namespace Tako {

  RigidbodyComponent::RigidbodyComponent() = default;

  RigidbodyComponent::~RigidbodyComponent()
  {
    EndPlay();
  }

  void RigidbodyComponent::Awake()
  {
    isPlaying_ = true;
    bodyType_  = type_;
    std::vector<PhysicsWorld::Shape> shapes;
    colliders_.clear();
    CollectShapes(*GetOwner(), Mat4x4::Inverse(GetOwner()->GetWorldMatrix()), shapes);

    PhysicsWorld* physics = PhysicsWorld::GetInstance();
    body_ = physics->CreateBody(MakeSettings(), shapes);
    if (!GetOwner()->IsActiveInHierarchy()) {
      physics->SetBodyEnabled(body_, false);
    }
  }

  void RigidbodyComponent::EndPlay()
  {
    if (!isPlaying_) {
      return;
    }
    isPlaying_ = false;
    PhysicsWorld::GetInstance()->ClearIgnoredCollisions(GetOwner());
    PhysicsWorld::GetInstance()->DestroyBody(body_);
    body_ = PhysicsWorld::kInvalidBody;

    // 預かっていた Collider は、それぞれ自分のボディに戻る（Play 中の Rigidbody ではなくなったので、もう預けに来ない）
    const std::vector<ColliderComponent*> colliders = std::exchange(colliders_, {});
    for (ColliderComponent* collider : colliders) {
      collider->rigidbody_ = nullptr;
      collider->Refresh();
    }
  }

  void RigidbodyComponent::OnActiveChanged(bool active)
  {
    if (body_ != PhysicsWorld::kInvalidBody) {
      PhysicsWorld::GetInstance()->SetBodyEnabled(body_, active);
    }
  }

  void RigidbodyComponent::PostEditChange()
  {
    if (body_ == PhysicsWorld::kInvalidBody) {
      return;
    }
    PhysicsWorld* physics = PhysicsWorld::GetInstance();
    if (bodyType_ == type_) {
      physics->SetBodySettings(body_, MakeSettings());
      return;
    }
    // Dynamic か Kinematic かはボディの種類なので、作り直す
    physics->DestroyBody(body_);
    body_ = PhysicsWorld::kInvalidBody;
    Awake();
  }

#ifdef TAKO_EDITOR
  bool RigidbodyComponent::DrawCustomImGui()
  {
    bool changed = false;
    changed |= UI::CheckboxFlags("Freeze Position X", &lockedAxes_, PhysicsWorld::kLockPositionX);
    changed |= UI::CheckboxFlags("Freeze Position Y", &lockedAxes_, PhysicsWorld::kLockPositionY);
    changed |= UI::CheckboxFlags("Freeze Position Z", &lockedAxes_, PhysicsWorld::kLockPositionZ);
    changed |= UI::CheckboxFlags("Freeze Rotation X", &lockedAxes_, PhysicsWorld::kLockRotationX);
    changed |= UI::CheckboxFlags("Freeze Rotation Y", &lockedAxes_, PhysicsWorld::kLockRotationY);
    changed |= UI::CheckboxFlags("Freeze Rotation Z", &lockedAxes_, PhysicsWorld::kLockRotationZ);
    return changed;
  }
#endif

  void RigidbodyComponent::AddForce(const Vector3& force)
  {
    PhysicsWorld::GetInstance()->AddForce(body_, force);
  }

  void RigidbodyComponent::AddTorque(const Vector3& torque)
  {
    PhysicsWorld::GetInstance()->AddTorque(body_, torque);
  }

  void RigidbodyComponent::AddImpulse(const Vector3& impulse)
  {
    PhysicsWorld::GetInstance()->AddImpulse(body_, impulse);
  }

  void RigidbodyComponent::AddForceAtPosition(const Vector3& force, const Vector3& position)
  {
    PhysicsWorld::GetInstance()->AddForceAtPosition(body_, force, position);
  }

  void RigidbodyComponent::AddImpulseAtPosition(const Vector3& impulse, const Vector3& position)
  {
    PhysicsWorld::GetInstance()->AddImpulseAtPosition(body_, impulse, position);
  }

  void RigidbodyComponent::WakeUp()
  {
    PhysicsWorld::GetInstance()->WakeBody(body_);
  }

  void RigidbodyComponent::Sleep()
  {
    PhysicsWorld::GetInstance()->SleepBody(body_);
  }

  void RigidbodyComponent::RebuildShapes()
  {
    if (body_ == PhysicsWorld::kInvalidBody) {
      return;
    }
    std::vector<PhysicsWorld::Shape> shapes;
    colliders_.clear();
    CollectShapes(*GetOwner(), Mat4x4::Inverse(GetOwner()->GetWorldMatrix()), shapes);

    PhysicsWorld* physics = PhysicsWorld::GetInstance();
    physics->SetBodyShapes(body_, shapes);
    // 層・摩擦・跳ね返りは、最初の Collider の値なので、入れ直す
    physics->SetBodySettings(body_, MakeSettings());
  }

  void RigidbodyComponent::SetBodyType(BodyType type)
  {
    if (type_ != type) {
      type_ = type;
      PostEditChange();
    }
  }

  void RigidbodyComponent::SetLockedAxes(uint32_t lockedAxes)
  {
    if (lockedAxes_ != lockedAxes) {
      lockedAxes_ = lockedAxes;
      PostEditChange();
    }
  }

  void RigidbodyComponent::SetLinearVelocity(const Vector3& velocity)
  {
    PhysicsWorld::GetInstance()->SetLinearVelocity(body_, velocity);
  }

  void RigidbodyComponent::SetAngularVelocity(const Vector3& velocity)
  {
    PhysicsWorld::GetInstance()->SetAngularVelocity(body_, velocity);
  }

  void RigidbodyComponent::SetCenterOfMass(const Vector3& offset)
  {
    centerOfMass_ = offset;
    PostEditChange();
  }

  Vector3 RigidbodyComponent::GetLinearVelocity() const
  {
    return PhysicsWorld::GetInstance()->GetLinearVelocity(body_);
  }

  Vector3 RigidbodyComponent::GetAngularVelocity() const
  {
    return PhysicsWorld::GetInstance()->GetAngularVelocity(body_);
  }

  bool RigidbodyComponent::IsSleeping() const
  {
    return PhysicsWorld::GetInstance()->IsBodySleeping(body_);
  }

  void RigidbodyComponent::CollectShapes(Actor& actor, const Matrix4x4& toBody, std::vector<PhysicsWorld::Shape>& shapes)
  {
    for (const std::unique_ptr<Component>& component : actor.GetComponents()) {
      if (!component->IsA<ColliderComponent>()) {
        continue;
      }
      auto* collider = static_cast<ColliderComponent*>(component.get());
      if (!collider->isPlaying_ || collider->isTrigger_) {
        continue;
      }
      // 自分のボディを持っていた Collider（この Rigidbody より先に Awake した物など）は、ここで預かる
      if (RigidbodyComponent* previous = std::exchange(collider->rigidbody_, this); previous != this) {
        collider->DestroyOwnBody();
        if (previous) {
          previous->RebuildShapes();
        }
      }
      colliders_.push_back(collider);
      if (!collider->IsEnabled()) {
        continue;
      }
      // 三角形のメッシュは、動く剛体の形にできない（物理エンジンの決まり。Unity も同じ）
      if (type_ == BodyType::Dynamic && collider->shape_ == ColliderComponent::Shape::Mesh) {
        Logger::Log("Rigidbody: the Mesh collider of %s was skipped, because a Dynamic body cannot have a Mesh shape. Use Convex Hull, or set Body Type to Kinematic", actor.GetName().c_str());
        continue;
      }

      PhysicsWorld::Shape shape = collider->MakeShape();
      if (&actor != GetOwner()) {
        // 子の Actor の形は、このボディの Actor から見た位置・向き・scale に直す
        const Matrix4x4 relative = Mat4x4::Multiply(actor.GetWorldMatrix(), toBody);
        Vector3         translate;
        Vector3         rotate;
        Mat4x4::Decompose(relative, translate, rotate, shape.scale);
        shape.position = Mat4x4::Transform(relative, shape.position);
        shape.rotation = Quat::FromEuler(rotate);
      }
      shapes.push_back(shape);
    }
    for (Actor* child : actor.GetChildren()) {
      // Play 中の Rigidbody を持つ子は、別のボディになる
      const RigidbodyComponent* other = child->GetComponent<RigidbodyComponent>();
      if (!other || !other->isPlaying_) {
        CollectShapes(*child, toBody, shapes);
      }
    }
  }

  PhysicsWorld::BodySettings RigidbodyComponent::MakeSettings() const
  {
    PhysicsWorld::BodySettings settings;
    settings.kind  = type_ == BodyType::Kinematic ? PhysicsWorld::BodyKind::Kinematic : PhysicsWorld::BodyKind::Dynamic;
    settings.actor = GetOwner();
    // ボディごとに 1 つしか持てない値は、最初の Collider の物を使う
    if (!colliders_.empty()) {
      settings.layer       = colliders_.front()->layer_;
      settings.friction    = colliders_.front()->friction_;
      settings.restitution = colliders_.front()->restitution_;
    }
    settings.mass               = mass_;
    settings.linearDamping      = linearDamping_;
    settings.angularDamping     = angularDamping_;
    settings.gravityScale       = gravityScale_;
    settings.lockedAxes         = lockedAxes_;
    settings.isContinuous       = isContinuous_;
    settings.centerOfMassOffset = centerOfMass_;
    return settings;
  }

} // namespace Tako
