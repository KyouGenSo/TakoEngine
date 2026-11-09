#include "ColliderComponent.h"
#include "Actor.h"
#include "CollisionManager.h"
#include "OBBCollider.h"
#include "SphereCollider.h"

namespace Tako {

  namespace {

    /// <summary>
    /// 相手のコライダーが ColliderComponent の物かを dynamic_cast で見分けるための印
    /// </summary>
    class ComponentColliderLink {
    public: //メンバー関数
      explicit ComponentColliderLink(ColliderComponent* component) : component_(component) {}
      virtual ~ComponentColliderLink() = default;

      //=====================================
      //Getter
      //=====================================
      ColliderComponent* GetComponent() const { return component_; }

    private: //メンバー変数
      ColliderComponent* component_;
    };

    /// <summary>
    /// 当たりを ColliderComponent へ渡すコライダー
    /// </summary>
    template<class Base>
    class ComponentCollider final : public Base, public ComponentColliderLink {
    public: //メンバー関数
      explicit ComponentCollider(ColliderComponent* component) : ComponentColliderLink(component) {}

      void OnCollisionEnter(Collider* other) override { GetComponent()->Notify(other, &Component::OnCollisionEnter); }
      void OnCollisionStay(Collider* other) override { GetComponent()->Notify(other, &Component::OnCollisionStay); }
      void OnCollisionExit(Collider* other) override { GetComponent()->Notify(other, &Component::OnCollisionExit); }
    };

  } // namespace

  ColliderComponent::ColliderComponent() = default;

  ColliderComponent::~ColliderComponent()
  {
    EndPlay();
  }

  void ColliderComponent::BeginPlay()
  {
    if (shape_ == Shape::Box) {
      collider_ = std::make_unique<ComponentCollider<OBBCollider>>(this);
    } else {
      collider_ = std::make_unique<ComponentCollider<SphereCollider>>(this);
    }
    // Actor はフラットなので、Actor の Transform がそのままワールドの Transform
    collider_->SetTransform(&GetOwner()->GetTransform());
    collider_->SetOwner(GetOwner());
    ApplyShape();

    CollisionManager* manager = CollisionManager::GetInstance();
    if (layer_ == kActorLayer) {
      manager->SetCollisionMask(kActorLayer, kActorLayer, true);
    }
    manager->AddCollider(collider_.get());
  }

  void ColliderComponent::EndPlay()
  {
    if (!collider_) {
      return;
    }
    CollisionManager::GetInstance()->RemoveCollider(collider_.get());
    collider_.reset();
  }

  void ColliderComponent::PostEditChange()
  {
    // 形の種類を変えたときは次の BeginPlay から効く
    if (collider_) {
      ApplyShape();
    }
  }

  void ColliderComponent::Notify(Collider* other, void (Component::* handler)(Actor*))
  {
    auto*  link       = dynamic_cast<ComponentColliderLink*>(other);
    Actor* otherActor = link ? link->GetComponent()->GetOwner() : nullptr;
    // 受け手がコンポーネントを足しても壊れないよう添字で回す
    const auto& components = GetOwner()->GetComponents();
    for (size_t i = 0; i < components.size(); ++i) {
      (components[i].get()->*handler)(otherActor);
    }
  }

  void ColliderComponent::ApplyShape()
  {
    collider_->SetTypeID(layer_);
    if (auto* sphere = dynamic_cast<SphereCollider*>(collider_.get())) {
      sphere->SetRadius(radius_);
      sphere->SetOffset(offset_);
    } else if (auto* box = dynamic_cast<OBBCollider*>(collider_.get())) {
      box->SetSize(size_);
      box->SetOffset(offset_);
    }
  }

} // namespace Tako
