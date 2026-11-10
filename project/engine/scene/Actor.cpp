#include "Actor.h"
#include "BaseScene.h"
#include "JsonPropertyVisitor.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "Script.h"
#include "ScriptComponent.h"
#include "TypeRegistry.h"

#include <algorithm>

namespace Tako {

  Actor::Actor() = default;

  Actor::~Actor() = default;

  Component* Actor::AddComponent(std::unique_ptr<Component> component, std::string name)
  {
    Component* raw = component.get();
    raw->name_  = MakeUniqueName(std::move(name), *raw);
    raw->owner_ = this;
    components_.push_back(std::move(component));
    if (hasAwoken_) {
      raw->Awake();
    }
    if (hasBegunPlay_) {
      raw->BeginPlay();
    }
    return raw;
  }

  void Actor::RemoveComponent(Component* component)
  {
    if (std::ranges::find(pendingRemoves_, component) == pendingRemoves_.end()) {
      pendingRemoves_.push_back(component);
    }
  }

  void Actor::Destroy()
  {
    if (scene_) {
      scene_->Destroy(this);
    }
  }

  bool Actor::SetParent(Actor* parent, bool keepWorld)
  {
    for (const Actor* ancestor = parent; ancestor; ancestor = ancestor->parent_) {
      if (ancestor == this) { return false; }
    }
    if (parent == parent_) { return true; }
    const Matrix4x4 world = GetWorldMatrix();
    parent_   = parent;
    parentId_ = parent ? parent->id_ : 0;
    if (keepWorld) {
      const Matrix4x4 local = parent ? world * Mat4x4::Inverse(parent->GetWorldMatrix()) : world;
      Mat4x4::Decompose(local, transform_.translate, transform_.rotate, transform_.scale);
    }
    if (scene_) {
      scene_->RelinkHierarchy();
      scene_->MarkChanged();
    }
    return true;
  }

  void Actor::SetWorldPosition(const Vector3& position)
  {
    transform_.translate = parent_ ? Mat4x4::Transform(Mat4x4::Inverse(parent_->GetWorldMatrix()), position) : position;
  }

  void Actor::EndPlay()
  {
    if (!hasBegunPlay_) {
      return;
    }
    for (const std::unique_ptr<Component>& component : components_) {
      component->EndPlay();
    }
    hasBegunPlay_ = false;
    hasAwoken_    = false;
  }

  void Actor::Draw()
  {
    for (const std::unique_ptr<Component>& component : components_) {
      component->Draw();
    }
  }

  nlohmann::json Actor::Save()
  {
    nlohmann::json components = nlohmann::json::object();
    for (size_t i = 0; i < components_.size(); ++i) {
      Component& component = *components_[i];
      components[component.GetName()] = {
        { "type", component.GetType().name },
        { "order", i },
        { "properties", WriteProperties(component) },
      };
    }
    nlohmann::json json = {
      { "id", id_ },
      { "type", GetType().name },
      { "properties", WriteProperties(*this) },
      { "components", std::move(components) },
    };
    if (parentId_ != 0) {
      json["parent"] = parentId_;
    }
    return json;
  }

  void Actor::Load(const nlohmann::json& json)
  {
    if (auto it = json.find("id"); it != json.end() && it->is_number_unsigned()) {
      id_ = it->get<ActorId>();
    }
    parentId_ = json.value("parent", ActorId{ 0 });
    if (auto it = json.find("properties"); it != json.end()) {
      ReadProperties(*this, *it);
    }

    const bool wasPlaying = hasBegunPlay_;
    EndPlay();
    components_.clear();
    pendingRemoves_.clear();

    auto it = json.find("components");
    if (it == json.end() || !it->is_object()) {
      if (wasPlaying) { scene_->BeginPlayActors({ this }); }
      return;
    }
    // nlohmann のオブジェクトはキーの順に並ぶので、保存した順に並べ直してから作る
    std::vector<std::pair<int64_t, std::string>> order;
    for (const auto& [name, entry] : it->items()) {
      order.emplace_back(entry.value("order", int64_t{ 0 }), name);
    }
    std::ranges::sort(order);
    for (const auto& [index, name] : order) {
      const nlohmann::json& entry = (*it)[name];
      const std::string type = entry.value("type", std::string{});
      std::unique_ptr<TObject> object = TypeRegistry::GetInstance()->Create(type);
      if (!object || !object->IsA<Component>()) {
        Logger::Log("Actor \"%s\": component type \"%s\" is unknown and was skipped\n", name_.c_str(), type.c_str());
        continue;
      }
      Component* component = AddComponent(std::unique_ptr<Component>(static_cast<Component*>(object.release())), name);
      if (auto properties = entry.find("properties"); properties != entry.end()) {
        ReadProperties(*component, *properties);
      }
    }
    if (wasPlaying) { scene_->BeginPlayActors({ this }); }
  }

  Matrix4x4 Actor::GetWorldMatrix() const
  {
    const Matrix4x4 local = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
    return parent_ ? local * parent_->GetWorldMatrix() : local;
  }

  Transform Actor::GetWorldTransform() const
  {
    if (!parent_) { return transform_; }
    Transform world;
    Mat4x4::Decompose(GetWorldMatrix(), world.translate, world.rotate, world.scale);
    return world;
  }

  Vector3 Actor::GetWorldPosition() const
  {
    const Matrix4x4 world = GetWorldMatrix();
    return { world.m[3][0], world.m[3][1], world.m[3][2] };
  }

  Actor* Actor::FindChild(std::string_view name) const
  {
    for (Actor* child : children_) {
      if (child->GetName() == name) { return child; }
    }
    return nullptr;
  }

  Component* Actor::FindComponent(const std::string& name) const
  {
    for (const std::unique_ptr<Component>& component : components_) {
      if (component->GetName() == name) { return component.get(); }
    }
    return nullptr;
  }

  Actor* Actor::Resolve(const ActorRef& ref) const
  {
    return scene_ && ref.id != 0 ? scene_->FindActor(ref.id) : nullptr;
  }

  bool Actor::Has(const TypeInfo& type) const
  {
    if (FindScript(type)) { return true; }
    return std::ranges::any_of(components_, [&type](const std::unique_ptr<Component>& component) { return component->IsA(type); });
  }

  Script* Actor::FindScript(const TypeInfo& type) const
  {
    for (const std::unique_ptr<Component>& component : components_) {
      if (!component->IsA<ScriptComponent>()) { continue; }
      Script* script = static_cast<ScriptComponent*>(component.get())->GetScript();
      if (script && script->IsA(type)) { return script; }
    }
    return nullptr;
  }

  void Actor::FlushPending()
  {
    for (Component* removed : pendingRemoves_) {
      auto it = std::ranges::find_if(components_, [removed](const std::unique_ptr<Component>& c) { return c.get() == removed; });
      if (it == components_.end()) { continue; }
      if (hasBegunPlay_) { (*it)->EndPlay(); }
      components_.erase(it);
    }
    pendingRemoves_.clear();
  }

  std::string Actor::MakeUniqueName(std::string name, const Component& component) const
  {
    if (name.empty()) { name = component.GetType().name; }
    std::string candidate = name;
    for (int index = 1; FindComponent(candidate); ++index) {
      candidate = name + "_" + std::to_string(index);
    }
    return candidate;
  }

} // namespace Tako
