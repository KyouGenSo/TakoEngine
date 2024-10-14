#include "GameScene.h"
#include"Audio.h"
#include"ModelManager.h"
#include"Object3dBasic.h"
#include"TextureManager.h"
#include"SpriteBasic.h"

#ifdef _DEBUG
#include"ImGui.h"
#endif

void GameScene::Initialize()
{
	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

	// modelの読み込み
	ModelManager::GetInstance()->LoadModel("teapot.obj");
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// サウンドデータの読み込み
	soundDataHandle = Audio::GetInstance()->LoadWaveFile("fanfare.wav");
	voiceHandle = 0;

	bgmSH = Audio::GetInstance()->LoadWaveFile("playerBulletHit.wav");
	bgmVH = 0;

	// spriteの初期化
	for (uint32_t i = 0; i < spriteNum_; i++) {
		Sprite* sprite = new Sprite();
		if (i % 2 == 0)
			sprite->Initialize("resources/uvChecker.png");
		else
			sprite->Initialize("resources/checkerBoard.png");
		sprite->SetPos(Vector2(i * 500.0f, 0.0f));
		sprites_.push_back(sprite);
	}

	// object3dの初期化
	object3d_ = new Object3d();
	object3d_->Initialize();
	object3d_->SetModel("teapot.obj");
	object3d_->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));

	object3d2_ = new Object3d();
	object3d2_->Initialize();
	object3d2_->SetModel("plane.obj");
	object3d2_->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
}

void GameScene::Finalize()
{
	delete object3d_;
	delete object3d2_;

	for (uint32_t i = 0; i < spriteNum_; i++)
	{
		delete sprites_[i];
	}
}

void GameScene::Update()
{
	// Spriteの更新
	for (uint32_t i = 0; i < spriteNum_; i++) {
		sprites_[i]->Update();
	}

	object3d_->SetRotate(Vector3(0.0f, object3d_->GetRotate().y + 0.01f, 0.0f));

	object3d2_->SetRotate(Vector3(object3d2_->GetRotate().x + 0.01f, 0.0f, 0.0f));

	// Object3dの更新
	object3d_->Update();
	object3d2_->Update();
}

void GameScene::Draw()
{
	//-------------------Modelの描画-------------------//
	// 3Dモデル共通描画設定
	Object3dBasic::GetInstance()->SetCommonRenderSetting();

	// 3Dモデルの描画
	object3d_->Draw();
	object3d2_->Draw();

	//-------------------Modelの描画-------------------//


	//-------------------Spriteの描画-------------------//
	// スプライト共通描画設定
	SpriteBasic::GetInstance()->SetCommonRenderSetting();

	for (uint32_t i = 0; i < spriteNum_; i++)
	{
		// Spriteの描画
		sprites_[i]->Draw();
	}

	//-------------------Spriteの描画-------------------//
}

void GameScene::DrawImGui()
{
#ifdef _DEBUG
	ImGui::Begin("Audio");
	if (ImGui::Button("Play Fanfare")) {
		voiceHandle = Audio::GetInstance()->PlayWave(soundDataHandle, loopFlag, volume);
	}

	if (ImGui::Button("Stop Fanfare")) {
		Audio::GetInstance()->StopWave(voiceHandle);
	}

	if (ImGui::Button("Play BGM")) {
		bgmVH = Audio::GetInstance()->PlayWave(bgmSH, true, volume);
	}

	if (ImGui::Button("Stop BGM")) {
		Audio::GetInstance()->StopWave(bgmVH);
	}

	// set loop
	ImGui::Checkbox("Loop Flag", &loopFlag);

	// set volume
	ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);

	ImGui::End();
#endif // DEBUG
}
