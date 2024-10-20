#include "TitleScene.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "SpriteBasic.h"
#include "Input.h"
#include "Draw2D.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void TitleScene::Initialize()
{
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
	DebugCamera::GetInstance()->Set2D();
#endif

	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

	sprite_ = new Sprite();
	sprite_->Initialize("resources/uvChecker.png");
	sprite_->SetPos(Vector2(0.0f, 0.0f));

	triangle1Pos_[0] = Vector2(200.0f, 200.0f);
	triangle1Pos_[1] = Vector2(300.0f, 300.0f);
	triangle1Pos_[2] = Vector2(100.0f, 300.0f);

	triangle2Pos_[0] = Vector2(500.0f, 200.0f);
	triangle2Pos_[1] = Vector2(600.0f, 300.0f);
	triangle2Pos_[2] = Vector2(400.0f, 300.0f);

}

void TitleScene::Finalize()
{
	delete sprite_;
}

void TitleScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1)) {
		Draw2D::GetInstance()->SetDebug(!Draw2D::GetInstance()->GetDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_) {
		DebugCamera::GetInstance()->Update();
	}
#endif
	sprite_->Update();

	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene("game");
	}
}

void TitleScene::Draw()
{
	SpriteBasic::GetInstance()->SetCommonRenderSetting();
	//sprite_->Draw();

	Draw2D::GetInstance()->DrawTriangle(triangle1Pos_[0], triangle1Pos_[1], triangle1Pos_[2], Vector4(1.0f, 0.0f, 0.0f, 1.0f));

	Draw2D::GetInstance()->DrawTriangle(triangle2Pos_[0], triangle2Pos_[1], triangle2Pos_[2], Vector4(0.0f, 1.0f, 0.0f, 1.0f));

	Draw2D::GetInstance()->DrawLine(Vector2(100.0f, 100.0f), Vector2(600.0f, 100.0f), Vector4(1.0f, 1.0f, 1.0f, 1.0f));
}

void TitleScene::DrawImGui()
{
#ifdef _DEBUG

	ImGui::Begin("Sprite");

	// set sprite position
	ImGui::Text("Sprite Position");
	Vector2 spritePos = sprite_->GetPos();
	ImGui::DragFloat2("Position", &spritePos.x, 0.1f);
	sprite_->SetPos(spritePos);

	// set sprite size
	ImGui::Text("Sprite Size");
	Vector2 spriteSize = sprite_->GetSize();
	ImGui::DragFloat2("Size", &spriteSize.x, 0.1f);
	sprite_->SetSize(spriteSize);

	// set sprite rotation
	ImGui::Text("Sprite Rotation");
	float spriteRotation = sprite_->GetRotation();
	ImGui::DragFloat("Rotation", &spriteRotation, 0.1f);
	sprite_->SetRotation(spriteRotation);

	// set sprite color
	ImGui::Text("Sprite Color");
	Vector4 spriteColor = sprite_->GetColor();
	ImGui::ColorEdit4("Color", &spriteColor.x);
	sprite_->SetColor(spriteColor);

	ImGui::End();

	ImGui::Begin("Triangle1");

	// set triangle1 position
	ImGui::Text("Triangle1 Position");
	ImGui::DragFloat2("Position1", &triangle1Pos_[0].x, 0.1f);
	ImGui::DragFloat2("Position2", &triangle1Pos_[1].x, 0.1f);
	ImGui::DragFloat2("Position3", &triangle1Pos_[2].x, 0.1f);

	ImGui::End();

	ImGui::Begin("Triangle2");

	// set triangle2 position
	ImGui::Text("Triangle2 Position");
	ImGui::DragFloat2("Position1", &triangle2Pos_[0].x, 0.1f);
	ImGui::DragFloat2("Position2", &triangle2Pos_[1].x, 0.1f);
	ImGui::DragFloat2("Position3", &triangle2Pos_[2].x, 0.1f);

	ImGui::End();


#endif // _DEBUG

}
