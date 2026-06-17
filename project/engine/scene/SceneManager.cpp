#include "SceneManager.h"
#include "TransitionManager.h"
#include "transition/ITransitionEffect.h"
#include "transition/FadeTransition.h"
#include "transition/ScaleTransition.h"
#include <cassert>

namespace Tako {

  std::unique_ptr<SceneManager> SceneManager::instance_ = nullptr;

  SceneManager* SceneManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<SceneManager>(new SceneManager());
    }
    return instance_.get();
  }

  void SceneManager::Update()
  {
    // フェードアウト完了後に次シーンへ切り替え、フェードインを開始
    if (nextScene_) {
      if (TransitionManager::GetInstance()->IsFinished()) {
        if (scene_) {
          scene_->Finalize();
          scene_.reset();
        }

        scene_ = std::move(nextScene_);
        scene_->Initialize();

        TransitionManager::GetInstance()->Start(
          ITransitionEffect::FADE_IN, transitionTime_);

        nextScene_ = nullptr;
      }
    }

    if (scene_) {
      scene_->Update();
    }

    TransitionManager::GetInstance()->Update();
  }

  void SceneManager::Draw()
  {
    if (scene_) {
      scene_->Draw();
    }
  }

  void SceneManager::DrawWithoutEffect()
  {
    if (scene_) {
      scene_->DrawWithoutEffect();
    }
  }

  void SceneManager::DrawImGui()
  {
    if (scene_) {
      scene_->DrawImGui();
    }
  }

  void SceneManager::Finalize()
  {
    if (scene_) {
      scene_->Finalize();
      scene_.reset();
    }
    if (nextScene_) {
      nextScene_.reset();
    }

    instance_.reset();
  }

  void SceneManager::ChangeScene(const std::string& sceneName)
  {
    assert(m_sceneFactory_);

    // 予約済みなら無視。フェードアウトを開始し次シーンを生成
    if (nextScene_ == nullptr) {
      TransitionManager::GetInstance()->Start(
        ITransitionEffect::FADE_OUT, transitionTime_);
      nextScene_ = m_sceneFactory_->CreateScene(sceneName);
    }
  }

  void SceneManager::ChangeScene(const std::string& sceneName, float transitionTime)
  {
    assert(m_sceneFactory_);

    if (nextScene_ == nullptr) {
      TransitionManager::GetInstance()->Start(
        ITransitionEffect::FADE_OUT, transitionTime);
      nextScene_ = m_sceneFactory_->CreateScene(sceneName);
      transitionTime_ = transitionTime;
    }
  }

  void SceneManager::ChangeScene(const std::string& sceneName,
    TransitionManager::EffectType effectType,
    float transitionTime)
  {
    assert(m_sceneFactory_);

    if (nextScene_ == nullptr) {
      TransitionManager::GetInstance()->Start(
        ITransitionEffect::FADE_OUT, effectType, transitionTime);
      nextScene_ = m_sceneFactory_->CreateScene(sceneName);
      transitionTime_ = transitionTime;
    }
  }

  void SceneManager::ChangeScene(const std::string& sceneName,
    const std::string& effectName,
    float transitionTime)
  {
    assert(m_sceneFactory_);

    if (nextScene_ == nullptr) {
      TransitionManager::GetInstance()->Start(
        ITransitionEffect::FADE_OUT, effectName, transitionTime);
      nextScene_ = m_sceneFactory_->CreateScene(sceneName);
      transitionTime_ = transitionTime;
    }
  }

  void SceneManager::ChangeScene(const std::string& sceneName,
    std::unique_ptr<ITransitionEffect> effect,
    float transitionTime)
  {
    assert(m_sceneFactory_);

    if (nextScene_ == nullptr) {
      // 渡されたエフェクトを設定してからフェードアウト開始
      TransitionManager::GetInstance()->SetCurrentEffect(std::move(effect));
      TransitionManager::GetInstance()->Start(
        ITransitionEffect::FADE_OUT, transitionTime);
      nextScene_ = m_sceneFactory_->CreateScene(sceneName);
      transitionTime_ = transitionTime;
    }
  }

} // namespace Tako
