#include "ParticleSystemComponent.h"
#include "Actor.h"
#include "BaseScene.h"
#include "EmitterManager.h"
#include "Mat4x4Func.h"

#include <format>

namespace Tako {

  ParticleSystemComponent::~ParticleSystemComponent()
  {
    RemoveEmitter();
  }

  void ParticleSystemComponent::BeginPlay()
  {
    isPlaying_ = true;
    SetActive(playOnStart_);
  }

  void ParticleSystemComponent::EndPlay()
  {
    isPlaying_ = false;
    SetActive(false);
  }

  void ParticleSystemComponent::Draw()
  {
    EnsureLoaded();
    if (manager_ && !emitterName_.empty()) {
      manager_->SetEmitterPosition(emitterName_, Mat4x4::Transform(GetOwner()->GetWorldMatrix(), offset_));
    }
  }

  void ParticleSystemComponent::SetActive(bool active)
  {
    isActive_ = active;
    if (manager_ && !emitterName_.empty()) {
      manager_->SetEmitterActive(emitterName_, isPlaying_ && isActive_);
    }
  }

  std::shared_ptr<GPUParticleEmitter> ParticleSystemComponent::GetEmitter()
  {
    EnsureLoaded();
    return manager_ && !emitterName_.empty() ? manager_->GetEmitterByName(emitterName_) : nullptr;
  }

  void ParticleSystemComponent::EnsureLoaded()
  {
    BaseScene* scene = GetOwner()->GetScene();
    if (!scene || preset_ == loadedPreset_) {
      return;
    }
    RemoveEmitter();
    loadedPreset_ = preset_;
    if (preset_.empty()) {
      return;
    }
    // Undo などで作り直したコンポーネントと古いものが 1 フレーム重なるので、作るたびに違う名前にする
    static uint32_t serial = 0;
    manager_     = &scene->GetEmitterManager();
    emitterName_ = std::format("Actor{}:{}:{}", GetOwner()->GetId(), GetName(), ++serial);
    manager_->LoadPreset(preset_, emitterName_);
    if (manager_->HasEmitter(emitterName_)) {
      manager_->SetEmitterActive(emitterName_, isPlaying_ && isActive_);
    } else {
      emitterName_.clear();
    }
  }

  void ParticleSystemComponent::RemoveEmitter()
  {
    if (manager_ && !emitterName_.empty()) {
      manager_->RemoveEmitter(emitterName_);
    }
    emitterName_.clear();
  }

} // namespace Tako
