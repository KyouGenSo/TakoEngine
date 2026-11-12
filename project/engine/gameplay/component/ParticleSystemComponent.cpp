#include "ParticleSystemComponent.h"
#include "Actor.h"
#include "BaseScene.h"
#include "EmitterManager.h"
#include "Mat4x4Func.h"
#include "MeshComponent.h"
#include "Object3d.h"

#include <format>

namespace Tako {

  ParticleSystemComponent::~ParticleSystemComponent()
  {
    RemoveEmitter();
  }

  void ParticleSystemComponent::Awake()
  {
    isPlaying_ = true;
    // 他のスクリプトが BeginPlay から名前で引いて操作できるよう、BeginPlay より前に作って初期の状態にしておく
    EnsureLoaded();
    SetActive(playOnStart_);
  }

  void ParticleSystemComponent::EndPlay()
  {
    isPlaying_ = false;
    SetActive(false);
  }

  void ParticleSystemComponent::OnSiblingRemoved(Component* removed)
  {
    if (boundObject_ && removed->IsA<MeshComponent>()) {
      RemoveEmitter();
      loadedPreset_.clear();
    }
  }

  void ParticleSystemComponent::Draw()
  {
    // 結び付けた先のモデルが替わったら、新しい形で作り直す
    if (boundObject_ && boundObject_->GetModel() != boundModel_) {
      RemoveEmitter();
      loadedPreset_.clear();
    }
    EnsureLoaded();
    // Edit 中はいつも Actor の位置へ置く（エディタで動かした結果が見えるように）。結び付けた物は、描画物の行列で位置が決まる
    if (manager_ && !activeName_.empty() && !boundObject_ && (followActor_ || !isPlaying_)) {
      manager_->SetEmitterPosition(activeName_, GetEmitPosition());
    }
  }

  void ParticleSystemComponent::OnActiveChanged([[maybe_unused]] bool active)
  {
    SetActive(isActive_);
  }

  void ParticleSystemComponent::SetActive(bool active)
  {
    isActive_ = active;
    if (manager_ && !activeName_.empty()) {
      manager_->SetEmitterActive(activeName_, isPlaying_ && isActive_ && GetOwner()->IsActiveInHierarchy());
    }
  }

  void ParticleSystemComponent::EmitTemporary(float lifeTime)
  {
    EnsureLoaded();
    if (!manager_ || activeName_.empty()) {
      return;
    }
    static uint32_t serial = 0;
    if (!boundObject_) {
      manager_->SetEmitterPosition(activeName_, GetEmitPosition());
    }
    manager_->CreateTemporaryEmitterFrom(activeName_, std::format("{}:temp{}", activeName_, ++serial), lifeTime);
  }

  std::shared_ptr<GPUParticleEmitter> ParticleSystemComponent::GetEmitter()
  {
    EnsureLoaded();
    return manager_ && !activeName_.empty() ? manager_->GetEmitterByName(activeName_) : nullptr;
  }

  void ParticleSystemComponent::EnsureLoaded()
  {
    BaseScene* scene = GetOwner()->GetScene();
    if (!scene || (preset_ == loadedPreset_ && emitterName_ == loadedEmitterName_ && bindToMesh_ == loadedBindToMesh_)) {
      return;
    }
    Object3d* bindTarget = nullptr;
    if (bindToMesh_ && !preset_.empty()) {
      MeshComponent* mesh = GetOwner()->GetComponent<MeshComponent>();
      bindTarget = mesh ? mesh->GetObject3d() : nullptr;
      // モデルがまだ無い間（コードで形を作る物、MeshComponent を後から足す物）は待つ
      if (!bindTarget || !bindTarget->GetModel()) {
        return;
      }
    }
    RemoveEmitter();
    loadedPreset_      = preset_;
    loadedEmitterName_ = emitterName_;
    loadedBindToMesh_  = bindToMesh_;
    if (preset_.empty()) {
      return;
    }
    // 名前を決めていないときは、Undo などで作り直したコンポーネントと古いものが 1 フレーム重なるので、作るたびに違う名前にする
    static uint32_t serial = 0;
    manager_    = &scene->GetEmitterManager();
    activeName_ = emitterName_.empty() ? std::format("Actor{}:{}:{}", GetOwner()->GetId(), GetName(), ++serial) : emitterName_;
    if (bindTarget) {
      manager_->LoadPreset(preset_, activeName_, bindTarget);
    } else {
      manager_->LoadPreset(preset_, activeName_);
    }
    created_ = manager_->GetEmitterByName(activeName_);
    if (created_.expired()) {
      activeName_.clear();
      return;
    }
    manager_->SetEmitterActive(activeName_, isPlaying_ && isActive_ && GetOwner()->IsActiveInHierarchy());
    if (bindTarget) {
      boundObject_ = bindTarget;
      boundModel_  = bindTarget->GetModel();
    } else {
      manager_->SetEmitterPosition(activeName_, GetEmitPosition());
    }
  }

  void ParticleSystemComponent::RemoveEmitter()
  {
    // 決めた名前のエミッターは、作り直した別のコンポーネントの物に替わっていることがある。自分が作った物だけを消す
    if (manager_ && !activeName_.empty()) {
      const std::shared_ptr<GPUParticleEmitter> mine = created_.lock();
      if (mine && manager_->GetEmitterByName(activeName_) == mine) {
        manager_->RemoveEmitter(activeName_);
      }
    }
    activeName_.clear();
    created_.reset();
    boundObject_ = nullptr;
    boundModel_  = nullptr;
  }

  Vector3 ParticleSystemComponent::GetEmitPosition() const
  {
    return Mat4x4::Transform(GetOwner()->GetWorldMatrix(), offset_);
  }

} // namespace Tako
