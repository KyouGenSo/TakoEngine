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
    if (manager_ && !emitterName_.empty()) {
      manager_->SetEmitterActive(emitterName_, playOnStart_);
    }
  }

  void ParticleSystemComponent::EndPlay()
  {
    isPlaying_ = false;
    if (manager_ && !emitterName_.empty()) {
      manager_->SetEmitterActive(emitterName_, false);
    }
  }

  void ParticleSystemComponent::Draw()
  {
    BaseScene* scene = GetOwner()->GetScene();
    if (!scene) {
      return;
    }
    if (preset_ != loadedPreset_) {
      RemoveEmitter();
      loadedPreset_ = preset_;
      if (!preset_.empty()) {
        // Undo などで作り直したコンポーネントと古いものが 1 フレーム重なるので、作るたびに違う名前にする
        static uint32_t serial = 0;
        manager_     = &scene->GetEmitterManager();
        emitterName_ = std::format("Actor{}:{}:{}", GetOwner()->GetId(), GetName(), ++serial);
        manager_->LoadPreset(preset_, emitterName_);
        if (manager_->HasEmitter(emitterName_)) {
          manager_->SetEmitterActive(emitterName_, isPlaying_ && playOnStart_);
        } else {
          emitterName_.clear();
        }
      }
    }
    if (manager_ && !emitterName_.empty()) {
      manager_->SetEmitterPosition(emitterName_, Mat4x4::Transform(GetOwner()->GetWorldMatrix(), offset_));
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
