#include "Actor.h"
#include "BaseScene.h"
#include "JsonPropertyVisitor.h"
#include "Logger.h"
#include "Mat4x4Func.h"
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

  void Actor::BeginPlay()
  {
    hasBegunPlay_ = true;
    // BeginPlay の中で足されたものは AddComponent が BeginPlay を呼ぶので、最初にあった分だけ回す
    const size_t count = components_.size();
    for (size_t i = 0; i < count; ++i) {
      components_[i]->BeginPlay();
    }
  }

  void Actor::Tick(float deltaTime)
  {
    // Tick の中で足されても要素の参照が壊れないよう添字で回す
    for (size_t i = 0; i < components_.size(); ++i) {
      components_[i]->Tick(deltaTime);
    }
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
    return {
      { "id", id_ },
      { "type", GetType().name },
      { "properties", WriteProperties(*this) },
      { "components", std::move(components) },
    };
  }

  void Actor::Load(const nlohmann::json& json)
  {
    if (auto it = json.find("id"); it != json.end() && it->is_number_unsigned()) {
      id_ = it->get<ActorId>();
    }
    if (auto it = json.find("properties"); it != json.end()) {
      ReadProperties(*this, *it);
    }

    const bool wasPlaying = hasBegunPlay_;
    EndPlay();
    components_.clear();
    pendingRemoves_.clear();

    auto it = json.find("components");
    if (it == json.end() || !it->is_object()) {
      if (wasPlaying) { BeginPlay(); }
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
    if (wasPlaying) { BeginPlay(); }
  }

  Matrix4x4 Actor::GetWorldMatrix() const
  {
    return Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
  }

  Component* Actor::FindComponent(const std::string& name) const
  {
    for (const std::unique_ptr<Component>& component : components_) {
      if (component->GetName() == name) { return component.get(); }
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
