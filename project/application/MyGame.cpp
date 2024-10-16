#include "MyGame.h"
#include"Audio.h"
#include"Input.h"
#include "SceneFactory.h"
#include "SceneManager.h"
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
	// 描画前の処理
	dx12_->BeginDraw();
	srvManager_->BeginDraw();

	//-------------------ImGui-------------------//
#ifdef _DEBUG
	imguiManager_->Begin();

	SceneManager::GetInstance()->DrawImGui();

	imguiManager_->End();
#endif
	//-------------------ImGui-------------------//

	// シーンの描画
	SceneManager::GetInstance()->Draw();


#ifdef _DEBUG
	//imguiの描画
	imguiManager_->Draw();
#endif

	// 描画後の処理
	dx12_->EndDraw();
}
