#include "SceneManager.h"

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

		// 次のシーンの予約を解除
		nextScene_ = nullptr;
	}

	if (scene_)
	{
		// シーンの更新
		scene_->Update();
	}
}

void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
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
		delete scene_;
	}
	if (nextScene_)
	{
		delete nextScene_;
	}
}
