
// Engineのヘッダーファイル
#include"WinApp.h"
#include"DX12Basic.h"
#include"Input.h"
#include"Audio.h"
#include"SpriteBasic.h"
#include"Sprite.h"
#include"D3DResourceLeakCheker.h"
#include"TextureManager.h"
#include"Object3d.h"
#include"Object3dBasic.h"
#include"Model.h"
#include"ModelManager.h"
#include"Camera.h"
#include"SrvManager.h"

#ifdef _DEBUG
#include"ImGuiManager.h"
#endif

// ComPtrのエイリアス
template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// リソースリークチェッカー
	//D3DResourceLeakCheker d3dResourceLeakCheker;

#pragma region ウィンドウの初期化-------------------------------------------------------------------------------------------------------------------
	//ウィンドウクラスの初期化
	//WinApp * winApp = new WinApp();
	//winApp->Initialize();
#pragma endregion


#pragma region 基盤システムの初期化-------------------------------------------------------------------------------------------------------------------
	// DX12の初期化
	//DX12Basic * dx12 = new DX12Basic();
	//dx12->Initialize(winApp);

	// ImGuiManagerの初期化
#ifdef _DEBUG
	//ImGuiManager* imguiManager = new ImGuiManager();
	//imguiManager->Initialize(winApp, dx12);
#endif

	// SRVマネージャーの初期化
	//SrvManager* srvManager = new SrvManager();
	//srvManager->Initialize(dx12);

	// TextureManagerの初期化
	//TextureManager::GetInstance()->Initialize(dx12, srvManager);

	// ModelManagerの初期化
	//ModelManager::GetInstance()->Initialize(dx12);

	// Sprite共通クラスの初期化
	//SpriteBasic* spriteBasic = new SpriteBasic();
	//spriteBasic->Initialize(dx12);

	// Object共通クラスの初期化
	//Object3dBasic* object3dBasic = new Object3dBasic();
	//object3dBasic->Initialize(dx12);

	// デフォルトカメラの初期化
	//Camera* defaultCamera = new Camera();
	//defaultCamera->SetRotate(Vector3(0.3f, 0.0f, 0.0f));
	//defaultCamera->SetTranslate(Vector3(0.0f, 4.0f, -10.0f));
	// デフォルトカメラを設定
	//object3dBasic->SetDefaultCamera(defaultCamera);
#pragma endregion


#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	//Input* input = new Input();
	//input->Initialize(winApp);

	//Audioの初期化
	//Audio* audio = new Audio();
	//audio->Initialize("resources/Sound/");

	//// サウンドデータの読み込み
	//uint32_t soundDataHandle = audio->LoadWaveFile("fanfare.wav");
	//uint32_t voiceHandle = 0;

	//uint32_t bgmSH = audio->LoadWaveFile("playerBulletHit.wav");
	//uint32_t bgmVH = 0;


	//float volume = 1.0f;
	//bool loopFlag = false;
#pragma endregion


#pragma region Sprite初期化-------------------------------------------------------------------------------------------------------------------

	//// textureの読み込み
	//TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	//TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

	//// スプライトの数
	//uint32_t spriteNum = 2;

	//std::vector<Sprite*> sprites;

	//for (uint32_t i = 0; i < spriteNum; i++) {
	//	Sprite* sprite = new Sprite();
	//	if (i % 2 == 0)
	//		sprite->Initialize(spriteBasic, "resources/uvChecker.png");
	//	else
	//		sprite->Initialize(spriteBasic, "resources/checkerBoard.png");
	//	sprite->SetPos(Vector2(i * 500.0f, 0.0f));
	//	sprites.push_back(sprite);
	//}

#pragma endregion


#pragma region Model読み込み-------------------------------------------------------------------------------------------------------------------

	//ModelManager::GetInstance()->LoadModel("teapot.obj");

	//ModelManager::GetInstance()->LoadModel("plane.obj");

#pragma endregion


#pragma region OBject3d初期化-------------------------------------------------------------------------------------------------------------------

	//Object3d * object3d = new Object3d();
	//object3d->Initialize(object3dBasic);
	//object3d->SetModel("teapot.obj");
	//object3d->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));

	//Object3d* object3d2 = new Object3d();
	//object3d2->Initialize(object3dBasic);
	//object3d2->SetModel("plane.obj");
	//object3d2->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));

#pragma endregion


	//---------------------------------------------------GAMELOOP-----------------------------------------------------//

	// ウィンドウが閉じられるまでループ
	while (true)
	{
		// ウィンドウメッセージの取得
		if (winApp->ProcessMessage()) {
			// ウィンドウが閉じられたらループを抜ける
			break;
		}

		/// <summary>
		/// 更新処理
		/// </summary>

		// 入力情報の更新
		input->Update();


		// Spriteの更新
		for (uint32_t i = 0; i < spriteNum; i++) {
			sprites[i]->Update();
		}

		object3d->SetRotate(Vector3(0.0f, object3d->GetRotate().y + 0.01f, 0.0f));

		object3d2->SetRotate(Vector3(object3d2->GetRotate().x + 0.01f, 0.0f, 0.0f));

		// Object3dの更新
		object3d->Update();
		object3d2->Update();

		/// <summary>
		/// 更新処理
		/// </summary>




		/// <summary>
		/// 描画処理
		/// </summary>

		// 描画前の処理
		dx12->BeginDraw();
		srvManager->BeginDraw();

		//-------------------ImGui-------------------//
#ifdef _DEBUG
		imguiManager->Begin();

		ImGui::Begin("Audio");
		if (ImGui::Button("Play Fanfare")) {
			voiceHandle = audio->PlayWave(soundDataHandle, loopFlag, volume);
		}

		if (ImGui::Button("Stop Fanfare")) {
			audio->StopWave(voiceHandle);
		}

		if (ImGui::Button("Play BGM")) {
			bgmVH = audio->PlayWave(bgmSH, true, volume);
		}

		if (ImGui::Button("Stop BGM")) {
			audio->StopWave(bgmVH);
		}

		// set loop
		ImGui::Checkbox("Loop Flag", &loopFlag);

		// set volume
		ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);

		ImGui::End();

		imguiManager->End();
#endif
		//-------------------ImGui-------------------//


		//-------------------Modelの描画-------------------//
		// 3Dモデル共通描画設定
		object3dBasic->SetCommonRenderSetting();

		// カメラの更新
		defaultCamera->Update();

		// 3Dモデルの描画
		object3d->Draw();
		object3d2->Draw();

		//-------------------Modelの描画-------------------//


		//-------------------Spriteの描画-------------------//
		// スプライト共通描画設定
		spriteBasic->SetCommonRenderSetting();

		//for (uint32_t i = 0; i < spriteNum; i++)
		//{
		//	// Spriteの描画
		//	sprites[i]->Draw();
		//}

		//-------------------Spriteの描画-------------------//

#ifdef _DEBUG
		//imguiの描画
		imguiManager->Draw();
#endif

		// 描画後の処理
		dx12->EndDraw();

		/// <summary>
		/// 描画処理
		/// </summary>

	}

	//-------------------------------------------------GAMELOOP-----------------------------------------------------/

	// ModelManagerの終了処理
	ModelManager::GetInstance()->Finalize();

	// TextureManagerの終了処理
	TextureManager::GetInstance()->Finalize();

#ifdef _DEBUG
	// ImGuiManagerの終了処理
	imguiManager->Shutdown();
	delete imguiManager;
#endif

	// DX12の終了処理
	dx12->Finalize();

	// Audioの解放
	audio->Finalize();

	// pointerの解放
	delete input;
	delete dx12;
	delete spriteBasic;
	delete object3d;
	delete object3d2;
	delete defaultCamera;
	delete object3dBasic;
	delete srvManager;
	delete audio;

	for (uint32_t i = 0; i < spriteNum; i++)
	{
		delete sprites[i];
	}



	winApp->Finalize();

	// ウィンドウクラスの解放
	delete winApp;

	return 0;
}