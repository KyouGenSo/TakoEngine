#include "ColliderComponent.h"
#include "Actor.h"
#include "RigidbodyComponent.h"

#include <span>
#include <utility>

#ifdef TAKO_EDITOR
#include "EditorUI.h"
#include "imgui.h"

#include <format>
#endif

namespace Tako {

  ColliderComponent::ColliderComponent() = default;

  ColliderComponent::~ColliderComponent()
  {
    EndPlay();
  }

  void ColliderComponent::Awake()
  {
    isPlaying_ = true;
    Refresh();
  }

  void ColliderComponent::EndPlay()
  {
    if (!isPlaying_) {
      return;
    }
    isPlaying_ = false;
    DestroyOwnBody();
    // 預けていた Rigidbody から、自分の形を外してもらう
    if (RigidbodyComponent* rigidbody = std::exchange(rigidbody_, nullptr)) {
      rigidbody->RebuildShapes();
    }
  }

  void ColliderComponent::OnActiveChanged([[maybe_unused]] bool active)
  {
    SetActive(isActive_);
  }

  void ColliderComponent::PostEditChange()
  {
    Refresh();
  }

#ifdef TAKO_EDITOR
  bool ColliderComponent::DrawCustomImGui()
  {
    const std::vector<std::string>& names = PhysicsWorld::GetInstance()->GetLayerNames();
    auto label = [&](uint32_t layer) {
      return layer < names.size() && !names[layer].empty() ? std::format("{}: {}", layer, names[layer]) : std::format("{}: Layer {}", layer, layer);
    };
    bool changed = false;
    if (UI::BeginCombo("Layer", label(layer_).c_str())) {
      for (uint32_t layer = 0; layer < PhysicsWorld::kMaxLayers; ++layer) {
        if (ImGui::Selectable(label(layer).c_str(), layer == layer_) && layer != layer_) {
          layer_  = layer;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Layer names and which layers collide are set in Engine Settings > Physics. Every pair of layers collides until it is unchecked there");
    return changed;
  }
#endif

  void ColliderComponent::SetActive(bool active)
  {
    isActive_ = active;
    // 形は変わらないので、世界への出し入れだけを行う（Rigidbody に預けた形は、まとめ直してもらう）
    if (rigidbody_) {
      rigidbody_->RebuildShapes();
    } else if (body_ != PhysicsWorld::kInvalidBody) {
      PhysicsWorld::GetInstance()->SetBodyEnabled(body_, IsEnabled());
    }
  }

  void ColliderComponent::SetTrigger(bool isTrigger)
  {
    if (isTrigger_ != isTrigger) {
      isTrigger_ = isTrigger;
      Refresh();
    }
  }

  void ColliderComponent::SetShape(Shape shape)
  {
    if (shape_ != shape) {
      shape_ = shape;
      Refresh();
    }
  }

  void ColliderComponent::SetRadius(float radius)
  {
    // 同じ値なら、形を作り直さない（毎フレーム呼ぶコードがある）
    if (radius_ != radius) {
      radius_ = radius;
      Refresh();
    }
  }

  void ColliderComponent::SetHeight(float height)
  {
    if (height_ != height) {
      height_ = height;
      Refresh();
    }
  }

  void ColliderComponent::SetSize(const Vector3& size)
  {
    if (size_ != size) {
      size_ = size;
      Refresh();
    }
  }

  void ColliderComponent::SetOffset(const Vector3& offset)
  {
    if (offset_ != offset) {
      offset_ = offset;
      Refresh();
    }
  }

  void ColliderComponent::Refresh()
  {
    if (!isPlaying_) {
      return;
    }
    // 実体の形は、自分か親の Rigidbody に預ける。Trigger は、いつも自分のボディを持つ
    RigidbodyComponent* wanted = isTrigger_ ? nullptr : FindRigidbody();
    if (RigidbodyComponent* previous = std::exchange(rigidbody_, wanted); previous && previous != wanted) {
      previous->RebuildShapes();
    }
    if (rigidbody_) {
      DestroyOwnBody();
      rigidbody_->RebuildShapes();
      return;
    }

    PhysicsWorld*             physics = PhysicsWorld::GetInstance();
    const PhysicsWorld::Shape shape   = MakeShape();
    // Trigger かどうかはボディの種類なので、変わっていたら作り直す
    if (isBodyTrigger_ != isTrigger_) {
      DestroyOwnBody();
    }
    if (body_ == PhysicsWorld::kInvalidBody) {
      isBodyTrigger_ = isTrigger_;
      body_          = physics->CreateBody(MakeSettings(), std::span(&shape, 1));
    } else {
      physics->SetBodyShapes(body_, std::span(&shape, 1));
      physics->SetBodySettings(body_, MakeSettings());
    }
    physics->SetBodyEnabled(body_, IsEnabled());
  }

  void ColliderComponent::DestroyOwnBody()
  {
    if (body_ != PhysicsWorld::kInvalidBody) {
      PhysicsWorld::GetInstance()->DestroyBody(body_);
      body_ = PhysicsWorld::kInvalidBody;
    }
  }

  RigidbodyComponent* ColliderComponent::FindRigidbody() const
  {
    for (const Actor* actor = GetOwner(); actor; actor = actor->GetParent()) {
      if (RigidbodyComponent* rigidbody = actor->GetComponent<RigidbodyComponent>(); rigidbody && rigidbody->IsPlaying()) {
        return rigidbody;
      }
    }
    return nullptr;
  }

  bool ColliderComponent::IsEnabled() const
  {
    return isActive_ && GetOwner()->IsActiveInHierarchy();
  }

  PhysicsWorld::Shape ColliderComponent::MakeShape()
  {
    PhysicsWorld::Shape shape;
    switch (shape_) {
    case Shape::Sphere:  shape.type = PhysicsWorld::ShapeType::Sphere; break;
    case Shape::Box:     shape.type = PhysicsWorld::ShapeType::Box; break;
    case Shape::Capsule: shape.type = PhysicsWorld::ShapeType::Capsule; break;
    }
    shape.radius   = radius_;
    shape.height   = height_;
    shape.size     = size_;
    shape.position = offset_;
    shape.owner    = this;
    return shape;
  }

  PhysicsWorld::BodySettings ColliderComponent::MakeSettings() const
  {
    PhysicsWorld::BodySettings settings;
    settings.kind        = isTrigger_ ? PhysicsWorld::BodyKind::Trigger : PhysicsWorld::BodyKind::Static;
    settings.layer       = layer_;
    settings.actor       = GetOwner();
    settings.friction    = friction_;
    settings.restitution = restitution_;
    return settings;
  }

} // namespace Tako
