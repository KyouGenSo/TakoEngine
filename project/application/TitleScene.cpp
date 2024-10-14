#include "TitleScene.h"
#include"TextureManager.h"
#include"SpriteBasic.h"

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
}

void TitleScene::Draw()
{
	SpriteBasic::GetInstance()->SetCommonRenderSetting();
	sprite_->Draw();
}

void TitleScene::DrawImGui()
{
}
