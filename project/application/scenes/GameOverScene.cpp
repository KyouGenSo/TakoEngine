#include "GameOverScene.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include"ModelManager.h"
#include "ParticleManager.h"
#include "Input.h"
#include "Draw2D.h"
#include "Camera.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif



void GameOverScene::Initialize() {
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
#endif

	/// ================================== ///
	///              初期化処理              ///
	/// ================================== ///

	TextureManager::GetInstance()->LoadTexture("black_BG.png");
	TextureManager::GetInstance()->LoadTexture("gameOver_Text.png");
	TextureManager::GetInstance()->LoadTexture("title_text.png");

	// sprite
	backGround_ = std::make_unique<Sprite>();
	backGround_->Initialize("black_BG.png");
	backGround_->SetPos(backGround_Pos_);
	backGround_->SetSize({1280.0f, 720.0f});

	titleText_ = std::make_unique<Sprite>();
	titleText_->Initialize("gameOver_Text.png");
	titleText_->SetPos(title_Pos_);

	pressButtonText_ = std::make_unique<Sprite>();
	pressButtonText_.get()->Initialize("title_text.png");
	pressButtonText_->SetPos(pressButton_Pos_);
	pressButtonText_->SetSize({250.0f, 50.0f});
}

void GameOverScene::Finalize()
{
}

void GameOverScene::Update() {
	backGround_->Update();
	titleText_->Update();
	pressButtonText_->Update();


	if (Input::GetInstance()->TriggerKey(DIK_RETURN))
	{
		SceneManager::GetInstance()->ChangeScene("title");
	}
}

void GameOverScene::Draw() {
/// ================================== ///
///              描画処理               ///
/// ================================== ///
//------------------背景Spriteの描画------------------//
// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();



	//--------------------------------------------------//


	//-------------------Modelの描画-------------------//
	// 3Dモデル共通描画設定
	Object3dBasic::GetInstance()->SetCommonRenderSetting();


	//------------------------------------------------//


	//------------------前景Spriteの描画------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();

	// 背景スプライト描画
	backGround_->Draw();

	// タイトルスプライト描画
	titleText_->Draw();

	// プレスボタンテキストスプライト描画
	pressButtonText_->Draw();

	//--------------------------------------------------//

}

void GameOverScene::DrawImGui()
{
#ifdef _DEBUG

#endif // DEBUG
}
