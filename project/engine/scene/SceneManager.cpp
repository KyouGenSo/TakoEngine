#include "SceneManager.h"
#include "TransitionManager.h"
#include "transition/ITransitionEffect.h"
#include "transition/FadeTransition.h"
#include "transition/ScaleTransition.h"
#include "Logger.h"
#include <algorithm>
#include <cassert>

namespace Tako {

  std::unique_ptr<SceneManager> SceneManager::instance_ = nullptr;

  SceneManager* SceneManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<SceneManager>(Token{});
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
        currentSceneName_ = std::move(nextSceneName_);
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
    ChangeScene(sceneName, transitionTime_);
  }

  void SceneManager::ChangeScene(const std::string& sceneName, float transitionTime)
  {
    if (ReserveNextScene(sceneName, transitionTime)) {
      TransitionManager::GetInstance()->Start(ITransitionEffect::FADE_OUT, transitionTime);
    }
  }

  void SceneManager::ChangeScene(const std::string& sceneName,
    TransitionManager::EffectType effectType,
    float transitionTime)
  {
    if (ReserveNextScene(sceneName, transitionTime)) {
      TransitionManager::GetInstance()->Start(ITransitionEffect::FADE_OUT, effectType, transitionTime);
    }
  }

  void SceneManager::ChangeScene(const std::string& sceneName,
    std::unique_ptr<ITransitionEffect> effect,
    float transitionTime)
  {
    if (ReserveNextScene(sceneName, transitionTime)) {
      TransitionManager::GetInstance()->SetCurrentEffect(std::move(effect));
      TransitionManager::GetInstance()->Start(ITransitionEffect::FADE_OUT, transitionTime);
    }
  }

  void SceneManager::ChangeToStartupScene()
  {
    const std::vector<std::string> names = GetSceneNames();
    // 名前一覧を返さないファクトリーでは確かめようがないので、設定された名前をそのまま使う（不明ならファクトリーが失敗を返す）
    const auto exists = [&names](const std::string& name) {
      return !name.empty() && (names.empty() || std::ranges::find(names, name) != names.end());
    };
    for (const std::string& name : { startupSceneOverride_, startupScene_ }) {
      if (exists(name)) {
        ChangeScene(name, 0.0f);
        return;
      }
    }
    if (!names.empty()) {
      ChangeScene(names.front(), 0.0f);
    }
  }

  bool SceneManager::ReserveNextScene(const std::string& sceneName, float transitionTime)
  {
    assert(sceneFactory_);

    if (nextScene_) {
      return false;
    }

    // 生成に失敗した場合はフェードアウトを始めない（フェードインに切り替わらず暗転したままになるため）
    nextScene_ = sceneFactory_->CreateScene(sceneName);
    if (!nextScene_) {
      Logger::Log("SceneManager: failed to create scene \"" + sceneName + "\"\n");
      return false;
    }

    nextSceneName_ = sceneName;
    transitionTime_ = transitionTime;
    return true;
  }

} // namespace Tako
