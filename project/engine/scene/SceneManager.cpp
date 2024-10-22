#include "SceneManager.h"
#include "Transition.h"
#include <cassert>

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
		if (Transition::GetInstance()->IsFinished())
		{
			// 現在のシーンがある場合
			if (scene_)
			{
				// 現在のシーンの終了処理
				scene_->Finalize();
				delete scene_;
			}

			// シーンの切り替え
			scene_ = nextScene_;

			// シーンの初期化
			scene_->Initialize();

			// シーン遷移アニメーションの開始
			Transition::GetInstance()->Start(Transition::FADE_IN, Transition::FADE, transitionTime_);

			// 次のシーンの予約を解除
			nextScene_ = nullptr;
		}
	}

	if (scene_)
	{
		// シーンの更新
		scene_->Update();
	}

	Transition::GetInstance()->Update();
}

void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
	}

	Transition::GetInstance()->Draw();
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
		delete scene_;
	}
	if (nextScene_)
	{
		delete nextScene_;
	}

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
		Transition::GetInstance()->Start(Transition::FADE_OUT, Transition::FADE, transitionTime_);
		nextScene_ = m_sceneFactory_->CreateScene(sceneName);
	}

}

void SceneManager::ChangeScene(const std::string& sceneName, float transitionTime)
{
	assert(m_sceneFactory_);

	// 次のシーンを生成
	if (nextScene_ == nullptr)
	{
		Transition::GetInstance()->Start(Transition::FADE_OUT, Transition::FADE, transitionTime);
		nextScene_ = m_sceneFactory_->CreateScene(sceneName);
	}
}
