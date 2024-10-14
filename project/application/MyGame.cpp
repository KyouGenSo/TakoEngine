#include "MyGame.h"
#include"Audio.h"

void MyGame::Initialize()
{

	TakoFramework::Initialize();

#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	// 入力クラス
	input_ = new Input();
	input_->Initialize(winApp_);

	Audio::GetInstance()->Initialize("resources/Sound/");

#pragma endregion

	// ゲームシーンの初期化
	//gameScene_ = new GameScene();
	//gameScene_->Initialize();

	// タイトルシーンの初期化
	titleScene_ = new TitleScene();
	titleScene_->Initialize();

}

void MyGame::Finalize()
{
	// Audioの解放
	Audio::GetInstance()->Finalize();

	delete input_;

	// ゲームシーンの終了処理
	titleScene_->Finalize();
	//delete gameScene_;
	delete titleScene_;

	TakoFramework::Finalize();
}

void MyGame::Update()
{

	TakoFramework::Update();

	// 入力情報の更新
	input_->Update();

	// カメラの更新
	defaultCamera_->Update();

	// ゲームシーンの更新
	titleScene_->Update();

}

void MyGame::Draw()
{
	// 描画前の処理
	dx12_->BeginDraw();
	srvManager_->BeginDraw();

	//-------------------ImGui-------------------//
#ifdef _DEBUG
	imguiManager_->Begin();

	titleScene_->DrawImGui();

	imguiManager_->End();
#endif
	//-------------------ImGui-------------------//


	// ゲームシーンの描画
	titleScene_->Draw();


#ifdef _DEBUG
	//imguiの描画
	imguiManager_->Draw();
#endif

	// 描画後の処理
	dx12_->EndDraw();
}
