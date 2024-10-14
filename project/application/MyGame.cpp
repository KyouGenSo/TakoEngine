#include "MyGame.h"

void MyGame::Initialize()
{

	TakoFramework::Initialize();

#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	// 入力クラス
	input_ = new Input();
	input_->Initialize(winApp_);

	audio_ = new Audio();
	audio_->Initialize("resources/Sound/");
#pragma endregion

	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

	// modelの読み込み
	ModelManager::GetInstance()->LoadModel("teapot.obj");
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// サウンドデータの読み込み
	soundDataHandle = audio_->LoadWaveFile("fanfare.wav");
	voiceHandle = 0;

	bgmSH = audio_->LoadWaveFile("playerBulletHit.wav");
	bgmVH = 0;

	// spriteの初期化
	for (uint32_t i = 0; i < spriteNum_; i++) {
		Sprite* sprite = new Sprite();
		if (i % 2 == 0)
			sprite->Initialize(spriteBasic_, "resources/uvChecker.png");
		else
			sprite->Initialize(spriteBasic_, "resources/checkerBoard.png");
		sprite->SetPos(Vector2(i * 500.0f, 0.0f));
		sprites_.push_back(sprite);
	}

	// object3dの初期化
	object3d_ = new Object3d();
	object3d_->Initialize(object3dBasic_);
	object3d_->SetModel("teapot.obj");
	object3d_->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));

	object3d2_ = new Object3d();
	object3d2_->Initialize(object3dBasic_);
	object3d2_->SetModel("plane.obj");
	object3d2_->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));

}

void MyGame::Finalize()
{
	// Audioの解放
	audio_->Finalize();

	delete input_;
	delete audio_;
	delete object3d_;
	delete object3d2_;

	for (uint32_t i = 0; i < spriteNum_; i++)
	{
		delete sprites_[i];
	}

	TakoFramework::Finalize();
}

void MyGame::Update()
{

	TakoFramework::Update();

	// 入力情報の更新
	input_->Update();

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

void MyGame::Draw()
{
	// 描画前の処理
	dx12_->BeginDraw();
	srvManager_->BeginDraw();

	//-------------------ImGui-------------------//
#ifdef _DEBUG
	imguiManager_->Begin();

	ImGui::Begin("Audio");
	if (ImGui::Button("Play Fanfare")) {
		voiceHandle = audio_->PlayWave(soundDataHandle, loopFlag, volume);
	}

	if (ImGui::Button("Stop Fanfare")) {
		audio_->StopWave(voiceHandle);
	}

	if (ImGui::Button("Play BGM")) {
		bgmVH = audio_->PlayWave(bgmSH, true, volume);
	}

	if (ImGui::Button("Stop BGM")) {
		audio_->StopWave(bgmVH);
	}

	// set loop
	ImGui::Checkbox("Loop Flag", &loopFlag);

	// set volume
	ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);

	ImGui::End();

	imguiManager_->End();
#endif
	//-------------------ImGui-------------------//


	//-------------------Modelの描画-------------------//
	// 3Dモデル共通描画設定
	object3dBasic_->SetCommonRenderSetting();

	// カメラの更新
	defaultCamera_->Update();

	// 3Dモデルの描画
	object3d_->Draw();
	object3d2_->Draw();

	//-------------------Modelの描画-------------------//


	//-------------------Spriteの描画-------------------//
	// スプライト共通描画設定
	spriteBasic_->SetCommonRenderSetting();

	//for (uint32_t i = 0; i < spriteNum_; i++)
	//{
	//	// Spriteの描画
	//	sprites_[i]->Draw();
	//}

	//-------------------Spriteの描画-------------------//

#ifdef _DEBUG
		//imguiの描画
	imguiManager_->Draw();
#endif

	// 描画後の処理
	dx12_->EndDraw();
}
