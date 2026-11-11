#include "SceneManager.h"
#include "TransitionManager.h"
#include "transition/ITransitionEffect.h"
#include "transition/FadeTransition.h"
#include "transition/ScaleTransition.h"
#include "FrameTimer.h"
#include "Logger.h"

#include <json.hpp>

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

  SceneManager::SceneManager(Token) {}

  SceneManager::~SceneManager() = default;

  void SceneManager::UpdateAlways()
  {
    const PlayRequest request = std::exchange(playRequest_, PlayRequest::None);
    if (request == PlayRequest::Play && scene_ && !isPlaying_) {
      playSnapshot_  = std::make_unique<nlohmann::json>(scene_->Save());
      playSceneName_ = currentSceneName_;
      isPlaying_     = true;
      scene_->BeginPlay();
    } else if (request == PlayRequest::Stop && CanStop()) {
      isPlaying_ = false;
      // 起動から Play していた（Play 前の中身が無い）ならファイルから作り直す
      const std::string sceneName = playSceneName_.empty() ? currentSceneName_ : playSceneName_;
      ReloadSceneImmediate(sceneName, playSnapshot_.get());
      playSnapshot_.reset();
      playSceneName_.clear();
      sessionData_.reset();
    }

    if (scene_) {
      scene_->UpdateAlways();
    }
  }

  void SceneManager::Update()
  {
    // フェードアウト完了後に次シーンへ切り替え、フェードインを開始
    if (nextScene_) {
      if (TransitionManager::GetInstance()->IsFinished()) {
        if (scene_) {
          scene_->Shutdown();
          scene_.reset();
        }

        scene_ = std::move(nextScene_);
        currentSceneName_ = std::move(nextSceneName_);
        scene_->Start(currentSceneName_, nullptr, isPlaying_);

        TransitionManager::GetInstance()->Start(
          ITransitionEffect::FADE_IN, transitionTime_);

        nextScene_ = nullptr;
      }
    }

    if (scene_ && isPlaying_) {
      scene_->UpdateFrame(FrameTimer::GetInstance()->GetDeltaTime());
    }

    TransitionManager::GetInstance()->Update();
  }

  void SceneManager::Draw()
  {
    if (scene_) {
      scene_->DrawFrame();
    }
  }

  void SceneManager::Finalize()
  {
    if (scene_) {
      scene_->Shutdown();
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

  nlohmann::json& SceneManager::GetSessionData()
  {
    if (!sessionData_) {
      sessionData_ = std::make_unique<nlohmann::json>(nlohmann::json::object());
    }
    return *sessionData_;
  }

  void SceneManager::RequestPlay()
  {
    playRequest_ = PlayRequest::Play;
  }

  void SceneManager::RequestStop()
  {
    playRequest_ = PlayRequest::Stop;
  }

  std::vector<std::string> SceneManager::GetSceneNames() const
  {
    return BaseScene::GetSceneFileNames();
  }

  void SceneManager::ChangeToStartupScene()
  {
    const std::vector<std::string> names = GetSceneNames();
    for (const std::string& name : { startupSceneOverride_, startupScene_ }) {
      if (std::ranges::find(names, name) != names.end()) {
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
    if (nextScene_) {
      return false;
    }

    // 生成に失敗した場合はフェードアウトを始めない（フェードインに切り替わらず暗転したままになるため）
    nextScene_ = CreateScene(sceneName);
    if (!nextScene_) {
      Logger::Log("SceneManager: failed to create scene \"" + sceneName + "\"\n");
      return false;
    }

    nextSceneName_ = sceneName;
    transitionTime_ = transitionTime;
    return true;
  }

  std::unique_ptr<BaseScene> SceneManager::CreateScene(const std::string& sceneName) const
  {
    return BaseScene::HasSceneFile(sceneName) ? std::make_unique<BaseScene>() : nullptr;
  }

  void SceneManager::ReloadSceneImmediate(const std::string& sceneName, const nlohmann::json* snapshot)
  {
    std::unique_ptr<BaseScene> scene = CreateScene(sceneName);
    if (!scene) {
      Logger::Log("SceneManager: failed to recreate scene \"" + sceneName + "\"\n");
      return;
    }
    if (scene_) {
      scene_->Shutdown();
      scene_.reset();
    }
    scene_            = std::move(scene);
    currentSceneName_ = sceneName;
    scene_->Start(currentSceneName_, snapshot, isPlaying_);
  }

} // namespace Tako
