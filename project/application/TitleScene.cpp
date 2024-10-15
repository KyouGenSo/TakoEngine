#include "TitleScene.h"
#include "SceneManager.h"
#include"TextureManager.h"
#include"SpriteBasic.h"
#include"Input.h"

void TitleScene::Initialize()
{
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

	sprite_ = new Sprite();
	sprite_->Initialize("resources/uvChecker.png");
	sprite_->SetPos(Vector2(0.0f, 0.0f));

}

void TitleScene::Finalize()
{
	delete sprite_;
}

void TitleScene::Update()
{
	sprite_->Update();

	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene("game");
	}
}

void TitleScene::Draw()
{
	SpriteBasic::GetInstance()->SetCommonRenderSetting();
	sprite_->Draw();
}

void TitleScene::DrawImGui()
{
}
