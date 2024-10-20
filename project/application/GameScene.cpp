#include "GameScene.h"
#include "SceneManager.h"
#include"Audio.h"
#include"ModelManager.h"
#include"Object3dBasic.h"
#include"TextureManager.h"
#include"SpriteBasic.h"
#include"Input.h"
#include "DebugCamera.h"

#ifdef _DEBUG
#include"ImGui.h"
#include "DebugCamera.h"
#endif

void GameScene::Initialize()
{
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
	DebugCamera::GetInstance()->Set3D();
#endif

	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");

	// modelの読み込み
	ModelManager::GetInstance()->LoadModel("teapot.obj");

	// サウンドデータの読み込み
	soundDataHandle = Audio::GetInstance()->LoadWaveFile("fanfare.wav");
	voiceHandle = 0;


	// spriteの初期化
	sprite_ = new Sprite();
	sprite_->Initialize("resources/uvChecker.png");

	// object3dの初期化
	object3d_ = new Object3d();
	object3d_->Initialize();
	object3d_->SetModel("teapot.obj");
	object3d_->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));
}

void GameScene::Finalize()
{
	delete object3d_;
	delete sprite_;
}

void GameScene::Update()
{
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_F1))
	{
		Object3dBasic::GetInstance()->SetDebug(!Object3dBasic::GetInstance()->GetDebug());
		isDebug_ = !isDebug_;
	}

	if (isDebug_)
	{
		DebugCamera::GetInstance()->Update();
	}
#endif

	// Spriteの更新
	sprite_->Update();

	object3d_->SetRotate(Vector3(0.0f, object3d_->GetRotate().y + 0.01f, 0.0f));

	if (Input::GetInstance()->PushKey(DIK_UP))
	{
		object3d_->SetTranslate(Vector3(object3d_->GetTranslate().x, object3d_->GetTranslate().y, object3d_->GetTranslate().z + 0.05f));
	}

	if (Input::GetInstance()->PushKey(DIK_DOWN))
	{
		object3d_->SetTranslate(Vector3(object3d_->GetTranslate().x, object3d_->GetTranslate().y, object3d_->GetTranslate().z - 0.05f));
	}

	if (Input::GetInstance()->PushKey(DIK_LEFT))
	{
		object3d_->SetTranslate(Vector3(object3d_->GetTranslate().x - 0.05f, object3d_->GetTranslate().y, object3d_->GetTranslate().z));
	}

	if (Input::GetInstance()->PushKey(DIK_RIGHT))
	{
		object3d_->SetTranslate(Vector3(object3d_->GetTranslate().x + 0.05f, object3d_->GetTranslate().y, object3d_->GetTranslate().z));
	}

	// Object3dの更新
	object3d_->Update();

	// シーン遷移
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene("title");
	}
}

void GameScene::Draw()
{
	//-------------------Modelの描画-------------------//
	// 3Dモデル共通描画設定
	Object3dBasic::GetInstance()->SetCommonRenderSetting();

	// 3Dモデルの描画
	object3d_->Draw();

	//-------------------Modelの描画-------------------//


	//-------------------Spriteの描画-------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();

	// Spriteの描画
	sprite_->Draw();

	//-------------------Spriteの描画-------------------//
}

void GameScene::DrawImGui()
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

	ImGui::Begin("Audio");
	if (ImGui::Button("Play Fanfare")) {
		voiceHandle = Audio::GetInstance()->PlayWave(soundDataHandle, loopFlag, volume);
	}

	if (ImGui::Button("Stop Fanfare")) {
		Audio::GetInstance()->StopWave(voiceHandle);
	}

	// set loop
	ImGui::Checkbox("Loop Flag", &loopFlag);

	// set volume
	ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);

	ImGui::End();
#endif // DEBUG
}
