#include "MyGame.h"
#include"Audio.h"
#include"Input.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "PostEffect.h"
#include "Draw2D.h"

void MyGame::Initialize()
{

	TakoFramework::Initialize();

#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	// 入力クラスの初期化
	Input::GetInstance()->Initialize(winApp_);

	// オーディオの初期化
	Audio::GetInstance()->Initialize("resources/Sound/");

#pragma endregion

	// シーンの初期化
	sceneFactory_ = new SceneFactory();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_);
	SceneManager::GetInstance()->ChangeScene("title");
}

void MyGame::Finalize()
{
	// Audioの解放
	Audio::GetInstance()->Finalize();

	// 入力クラスの解放
	Input::GetInstance()->Finalize();

	TakoFramework::Finalize();
}

void MyGame::Update()
{

	TakoFramework::Update();

	// 入力情報の更新
	Input::GetInstance()->Update();

	//　サウンドの更新
	Audio::GetInstance()->Update();

	// カメラの更新
	defaultCamera_->Update();
	
}

void MyGame::Draw()
{
	// 描画前の処理(レンダーテクスチャを描画対象に設定)
	dx12_->BeginDraw();

	/// ============================================= ///
	/// ------------------シーン描画-------------------///
	/// ============================================= ///

	// テクスチャ用のsrvヒープの設定
	SrvManager::GetInstance()->BeginDraw();

	// シーンの描画
	SceneManager::GetInstance()->Draw();



	/// ============================================= ///
	/// ------------------シーン描画-------------------///
	/// ============================================= ///

	// SwapChainを描画対象に設定
	dx12_->SetSwapChain();

	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///
#ifdef _DEBUG

	imguiManager_->Begin();

	SceneManager::GetInstance()->DrawImGui();

	Draw2D::GetInstance()->ImGui();

	imguiManager_->End();

	//imguiの描画
	imguiManager_->Draw();
#endif
	/// ========================================= ///
	///-------------------ImGui-------------------///
	/// ========================================= ///

	// 描画後の処理
	dx12_->EndDraw();
}
