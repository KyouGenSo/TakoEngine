#include "AnimatorComponent.h"
#include "Actor.h"
#include "MeshComponent.h"
#include "Model.h"
#include "Object3d.h"

namespace Tako {

  namespace {

    Object3d* FindTarget(const Actor& owner)
    {
      MeshComponent* mesh = owner.GetComponent<MeshComponent>();
      return mesh ? mesh->GetObject3d() : nullptr;
    }

  } // namespace

  void AnimatorComponent::BeginPlay()
  {
    Object3d* target = FindTarget(*GetOwner());
    if (target && playOnStart_ && !clip_.empty()) {
      target->GetAnimator().Play(clip_, "", { .speed = speed_ });
    }
  }

  void AnimatorComponent::EndPlay()
  {
    if (Object3d* target = FindTarget(*GetOwner())) {
      target->GetAnimator().StopAll();
    }
  }

  void AnimatorComponent::Draw()
  {
    const Object3d* target = FindTarget(*GetOwner());
    Model*          model  = target ? target->GetModel() : nullptr;
    // MeshComponent がモデルを読み直すと別の Model になるので、そのたびに渡し直す
    if (!model || (model == appliedModel_ && skeletalAnimation_ == appliedSkeletal_)) {
      return;
    }
    if (!skeletalAnimation_.empty() && model->HasAnimation()) {
      model->SetAnimation(skeletalAnimation_);
    }
    appliedModel_    = model;
    appliedSkeletal_ = skeletalAnimation_;
  }

} // namespace Tako
