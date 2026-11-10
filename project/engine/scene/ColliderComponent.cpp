#include "ColliderComponent.h"
#include "Actor.h"
#include "CollisionManager.h"
#include "OBBCollider.h"
#include "SphereCollider.h"

#ifdef _DEBUG
#include "imgui.h"

#include <format>
#endif

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
    collider_->SetOwner(GetOwner());
    collider_->SetActive(isActive_ && GetOwner()->IsActiveInHierarchy());
    LateTick(0.0f);
    ApplyShape();

    CollisionManager* manager = CollisionManager::GetInstance();
    if (layer_ == kActorLayer) {
      manager->SetCollisionMask(kActorLayer, kActorLayer, true);
    }
    manager->AddCollider(collider_.get());
  }

  void ColliderComponent::LateTick([[maybe_unused]] float deltaTime)
  {
    if (!collider_) {
      return;
    }
    // 親が無ければ Actor の Transform がそのままワールド（写さずに指すので、Tick の後の値がそのまま当たり判定に使われる）
    if (!GetOwner()->GetParent()) {
      collider_->SetTransform(&GetOwner()->GetTransform());
      return;
    }
    worldTransform_ = GetOwner()->GetWorldTransform();
    collider_->SetTransform(&worldTransform_);
  }

  void ColliderComponent::EndPlay()
  {
    if (!collider_) {
      return;
    }
    CollisionManager::GetInstance()->RemoveCollider(collider_.get());
    collider_.reset();
  }

  void ColliderComponent::OnActiveChanged(bool active)
  {
    if (collider_) {
      collider_->SetActive(isActive_ && active);
    }
  }

  void ColliderComponent::SetActive(bool active)
  {
    isActive_ = active;
    if (collider_) {
      collider_->SetActive(active && GetOwner()->IsActiveInHierarchy());
    }
  }

  void ColliderComponent::SetRadius(float radius)
  {
    radius_ = radius;
    PostEditChange();
  }

  void ColliderComponent::SetSize(const Vector3& size)
  {
    size_ = size;
    PostEditChange();
  }

  void ColliderComponent::SetOffset(const Vector3& offset)
  {
    offset_ = offset;
    PostEditChange();
  }

  void ColliderComponent::PostEditChange()
  {
    // 形の種類を変えたときは次の BeginPlay から効く
    if (collider_) {
      ApplyShape();
    }
  }

#ifdef _DEBUG
  bool ColliderComponent::DrawCustomImGui()
  {
    const std::vector<std::string>& names = CollisionManager::GetInstance()->GetLayerNames();
    auto label = [&](uint32_t layer) {
      return layer < names.size() && !names[layer].empty() ? std::format("{}: {}", layer, names[layer]) : std::format("{}: Layer {}", layer, layer);
    };
    bool changed = false;
    if (ImGui::BeginCombo("Layer", label(layer_).c_str())) {
      for (uint32_t layer = 0; layer < CollisionManager::kMaxLayers; ++layer) {
        if (ImGui::Selectable(label(layer).c_str(), layer == layer_) && layer != layer_) {
          layer_  = layer;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Layer names and which layers collide are set in Engine Settings > Physics. %u collides only with other actors by default", kActorLayer);
    return changed;
  }
#endif

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
