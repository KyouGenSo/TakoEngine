#include "ColliderComponent.h"
#include "Actor.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "MeshComponent.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
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
    meshData_.reset();
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
    // モデルの欄が変わったかもしれないので、頂点は作り直す
    meshData_.reset();
    Refresh();
  }

#ifdef TAKO_EDITOR
  bool ColliderComponent::DrawCustomImGui()
  {
    return DrawLayerCombo(layer_);
  }

  void ColliderComponent::DrawSelectionGizmo(const Vector3& cameraPosition)
  {
    // ponytail: Mesh と Convex Hull は、形を作るのが重いので描かない（Play 中は Collider Visibility で出る）。要るなら、作った形を覚えておく
    if (shape_ == Shape::Mesh || shape_ == Shape::ConvexHull) {
      return;
    }
    const PhysicsWorld::Shape shape = MakeShape();
    PhysicsWorld::GetInstance()->DrawShapes(*GetOwner(), std::span<const PhysicsWorld::Shape>(&shape, 1), cameraPosition);
  }

  bool ColliderComponent::DrawLayerCombo(uint32_t& layer)
  {
    const std::vector<std::string>& names = PhysicsWorld::GetInstance()->GetLayerNames();
    auto label = [&](uint32_t index) {
      return index < names.size() && !names[index].empty() ? std::format("{}: {}", index, names[index]) : std::format("{}: Layer {}", index, index);
    };
    bool changed = false;
    if (UI::BeginCombo("Layer", label(layer).c_str())) {
      for (uint32_t index = 0; index < PhysicsWorld::kMaxLayers; ++index) {
        if (ImGui::Selectable(label(index).c_str(), index == layer) && index != layer) {
          layer   = index;
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
    case Shape::Sphere:     shape.type = PhysicsWorld::ShapeType::Sphere; break;
    case Shape::Box:        shape.type = PhysicsWorld::ShapeType::Box; break;
    case Shape::Capsule:    shape.type = PhysicsWorld::ShapeType::Capsule; break;
    case Shape::Mesh:       shape.type = PhysicsWorld::ShapeType::Mesh; break;
    case Shape::ConvexHull: shape.type = PhysicsWorld::ShapeType::ConvexHull; break;
    }
    if (shape_ == Shape::Mesh || shape_ == Shape::ConvexHull) {
      if (!meshData_) {
        meshData_ = BuildMeshData();
      }
      shape.mesh = meshData_;
    }
    shape.radius   = radius_;
    shape.height   = height_;
    shape.size     = size_;
    shape.position = offset_;
    shape.owner    = this;
    return shape;
  }

  std::shared_ptr<const PhysicsWorld::MeshData> ColliderComponent::BuildMeshData() const
  {
    auto data = std::make_shared<PhysicsWorld::MeshData>();
    if (!model_.empty()) {
      if (const std::unique_ptr<Model> model = ModelManager::GetInstance()->GetModel(model_)) {
        model->AppendTriangles(Mat4x4::MakeIdentity(), data->positions, data->indices);
        model->Finalize();
      }
    } else if (MeshComponent* mesh = GetOwner()->GetComponent<MeshComponent>()) {
      if (const Model* model = mesh->GetObject3d()->GetModel()) {
        const Transform& offset = mesh->GetOffset();
        model->AppendTriangles(Mat4x4::MakeAffine(offset.scale, offset.rotate, offset.translate), data->positions, data->indices);
      }
    }
    if (data->positions.empty()) {
      Logger::Log("Collider: %s has no model for its Mesh / Convex Hull shape. Set Model, or add a Mesh component with a model", GetOwner()->GetName().c_str());
    }
    return data;
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
