#include "TransitionManager.h"
#include "transition/FadeTransition.h"
#include "transition/ScaleTransition.h"
#include "WinApp.h"
#include <cassert>

namespace Tako {

  std::unique_ptr<TransitionManager> TransitionManager::instance_ = nullptr;

  TransitionManager* TransitionManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<TransitionManager>(Token{});
    }
    return instance_.get();
  }

  void TransitionManager::Initialize()
  {
    currentEffect_ = CreateEffect(defaultEffectType_);
    if (currentEffect_) {
      currentEffect_->Initialize();
    }
  }

  void TransitionManager::Finalize()
  {
    currentEffect_.reset();
    instance_.reset();
  }

  void TransitionManager::Update()
  {
    if (currentEffect_) {
      currentEffect_->Update();
    }
  }

  void TransitionManager::Draw()
  {
    if (currentEffect_) {
      currentEffect_->Draw();
    }
  }

  std::unique_ptr<ITransitionEffect> TransitionManager::CreateEffect(EffectType type) const
  {
    switch (type) {
    case EffectType::Fade:
      return std::make_unique<FadeTransition>();

    case EffectType::BlackFade:
      return std::make_unique<FadeTransition>(Vector4(0.0f, 0.0f, 0.0f, 1.0f));

    case EffectType::ScaleExpand:
      return std::make_unique<ScaleTransition>();

    case EffectType::ScaleShrink:
      return std::make_unique<ScaleTransition>(
        Vector2(static_cast<float>(WinApp::clientWidth) * 0.5f, static_cast<float>(WinApp::clientHeight) * 0.5f), false);
    }
    return nullptr;
  }

  void TransitionManager::SetCurrentEffect(std::unique_ptr<ITransitionEffect> effect)
  {
    if (effect) {
      currentEffect_ = std::move(effect);
      currentEffect_->Initialize();
    }
  }

  void TransitionManager::SetCurrentEffect(EffectType type)
  {
    auto effect = CreateEffect(type);
    if (effect) {
      SetCurrentEffect(std::move(effect));
    }
  }

  ITransitionEffect* TransitionManager::GetCurrentEffect() const
  {
    return currentEffect_.get();
  }

  void TransitionManager::Start(ITransitionEffect::TransitionState state, float duration)
  {
    if (currentEffect_) {
      currentEffect_->Start(state, duration);
    }
  }

  void TransitionManager::Start(ITransitionEffect::TransitionState state, EffectType type, float duration)
  {
    SetCurrentEffect(type);
    if (currentEffect_) {
      currentEffect_->Start(state, duration);
    }
  }

  void TransitionManager::Stop()
  {
    if (currentEffect_) {
      currentEffect_->Stop();
    }
  }

  bool TransitionManager::IsFinished() const
  {
    if (currentEffect_) {
      return currentEffect_->IsFinished();
    }
    return true;
  }

} // namespace Tako