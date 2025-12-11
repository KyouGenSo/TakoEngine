#include "SceneManager.h"
#include "TransitionManager.h"
#include "transition/ITransitionEffect.h"
#include "transition/FadeTransition.h"
#include "transition/ScaleTransition.h"
#include <cassert>

namespace Tako {

SceneManager* SceneManager::instance_ = nullptr;

SceneManager* SceneManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new SceneManager();
	}
	return instance_;
}

void SceneManager::Update()
{
	// 次のシーンが予約されている場合
	if (nextScene_)
	{
		// シーン遷移アニメーションが終了している場合
		// TransitionManagerを使用（Transitionは内部でTransitionManagerを使用）
		if (TransitionManager::GetInstance()->IsFinished())
		{
			// 現在のシーンがある場合
			if (scene_)
			{
				// 現在のシーンの終了処理
				scene_->Finalize();
				scene_.reset();
			}

			// シーンの切り替え
			scene_ = std::move(nextScene_);

			// シーンの初期化
			scene_->Initialize();

			// シーン遷移アニメーションの開始（フェードイン）
			TransitionManager::GetInstance()->Start(
				ITransitionEffect::FADE_IN, transitionTime_);

			// 次のシーンの予約を解除
			nextScene_ = nullptr;
		}
	}

	if (scene_)
	{
		// シーンの更新
		scene_->Update();
	}

	// TransitionManagerの更新
	TransitionManager::GetInstance()->Update();
}

void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
	}
}

void SceneManager::DrawWithoutEffect()
{
  if (scene_)
  {
    scene_->DrawWithoutEffect();
  }
}

void SceneManager::DrawImGui()
{
	if (scene_)
	{
		scene_->DrawImGui();
	}
}

void SceneManager::Finalize()
{
	if (scene_)
	{
		scene_->Finalize();
		scene_.reset();
	}
	if (nextScene_)
	{
		nextScene_.reset();
	}

	// シングルトンインスタンスを削除
	if (instance_)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void SceneManager::ChangeScene(const std::string& sceneName)
{
	assert(m_sceneFactory_);

	// 次のシーンを生成
	if (nextScene_ == nullptr) {
		// TransitionManagerを使用（デフォルトはFade）
		TransitionManager::GetInstance()->Start(
			ITransitionEffect::FADE_OUT, transitionTime_);
		nextScene_ = m_sceneFactory_->CreateScene(sceneName);
	}
}

void SceneManager::ChangeScene(const std::string& sceneName, float transitionTime)
{
	assert(m_sceneFactory_);

	// 次のシーンを生成
	if (nextScene_ == nullptr)
	{
		// TransitionManagerを使用（デフォルトはFade）
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

	// 次のシーンを生成
	if (nextScene_ == nullptr)
	{
		// TransitionManagerを使用して指定されたエフェクトで開始
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

	// 次のシーンを生成
	if (nextScene_ == nullptr)
	{
		// TransitionManagerを使用して名前指定されたエフェクトで開始
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

	// 次のシーンを生成
	if (nextScene_ == nullptr)
	{
		// TransitionManagerに新しいエフェクトを設定してから開始
		TransitionManager::GetInstance()->SetCurrentEffect(std::move(effect));
		TransitionManager::GetInstance()->Start(
			ITransitionEffect::FADE_OUT, transitionTime);
		nextScene_ = m_sceneFactory_->CreateScene(sceneName);
		transitionTime_ = transitionTime;
	}
}

} // namespace Tako
