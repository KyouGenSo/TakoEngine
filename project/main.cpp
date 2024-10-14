#include<Windows.h>
#include<wrl.h>
#include<cstdint>
#include<string>
#include<fstream>
#include<sstream>
#include<format>
#include <unordered_map>
#include <cassert>
#include <vector>

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

#include "xaudio2.h"
#pragma comment(lib, "xaudio2.lib")

#include "Logger.h"
#include "StringUtility.h"

// Function includes
#include"Vector4.h"
#include"Vector2.h"
#include"Mat4x4Func.h"
#include"Vec3Func.h"

// ComPtrのエイリアス
template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


struct ChunkHeader {
	char id[4]; // チャンクのID
	uint32_t size; // チャンクのサイズ
};

struct RiffHeader {
	ChunkHeader chunk; // "RIFF"
	char type[4]; // "WAVE"
};

struct FormatChunk {
	ChunkHeader chunk; // "fmt "
	WAVEFORMATEX fmt; // 波形フォーマット
};

struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE* pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

//-----------------------------------------FUNCTION-----------------------------------------//

SoundData LoadWaveFile(const char* filename);

void SoundUnload(SoundData* soundData);

void SoundPlay(IXAudio2* xAudio2, const SoundData& soundData);

//-----------------------------------------FUNCTION-----------------------------------------//


//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// リソースリークチェッカー
	D3DResourceLeakCheker d3dResourceLeakCheker;

#pragma region ウィンドウの初期化-------------------------------------------------------------------------------------------------------------------
	//ウィンドウクラスの初期化
	WinApp * winApp = new WinApp();
	winApp->Initialize();
#pragma endregion


#pragma region 基盤システムの初期化-------------------------------------------------------------------------------------------------------------------
	// DX12の初期化
	DX12Basic * dx12 = new DX12Basic();
	dx12->Initialize(winApp);

	// ImGuiManagerの初期化
#ifdef _DEBUG
	ImGuiManager* imguiManager = new ImGuiManager();
	imguiManager->Initialize(winApp, dx12);
#endif

	// SRVマネージャーの初期化
	SrvManager* srvManager = new SrvManager();
	srvManager->Initialize(dx12);

	// TextureManagerの初期化
	TextureManager::GetInstance()->Initialize(dx12, srvManager);

	// ModelManagerの初期化
	ModelManager::GetInstance()->Initialize(dx12);

	// Sprite共通クラスの初期化
	SpriteBasic* spriteBasic = new SpriteBasic();
	spriteBasic->Initialize(dx12);

	// Object共通クラスの初期化
	Object3dBasic* object3dBasic = new Object3dBasic();
	object3dBasic->Initialize(dx12);

	// デフォルトカメラの初期化
	Camera* defaultCamera = new Camera();
	defaultCamera->SetRotate(Vector3(0.3f, 0.0f, 0.0f));
	defaultCamera->SetTranslate(Vector3(0.0f, 4.0f, -10.0f));
	// デフォルトカメラを設定
	object3dBasic->SetDefaultCamera(defaultCamera);
#pragma endregion


#pragma region 汎用機能初期化-------------------------------------------------------------------------------------------------------------------
	Input* input = new Input();
	input->Initialize(winApp);

	//Audioの初期化
	Audio* audio = new Audio();
	audio->Initialize("resources/");
	uint32_t soundDataHandle = audio->LoadWaveFile("fanfare.wav");
	uint32_t voiceHandle = 0;
	float volume = 1.0f;
	bool loopFlag = false;
#pragma endregion


#pragma region Sprite初期化-------------------------------------------------------------------------------------------------------------------

	// textureの読み込み
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

	// スプライトの数
	uint32_t spriteNum = 2;

	std::vector<Sprite*> sprites;

	for (uint32_t i = 0; i < spriteNum; i++) {
		Sprite* sprite = new Sprite();
		if (i % 2 == 0)
			sprite->Initialize(spriteBasic, "resources/uvChecker.png");
		else
			sprite->Initialize(spriteBasic, "resources/checkerBoard.png");
		sprite->SetPos(Vector2(i * 500.0f, 0.0f));
		sprites.push_back(sprite);
	}

#pragma endregion


#pragma region Model読み込み-------------------------------------------------------------------------------------------------------------------

	ModelManager::GetInstance()->LoadModel("teapot.obj");

	ModelManager::GetInstance()->LoadModel("plane.obj");

#pragma endregion


#pragma region OBject3d初期化-------------------------------------------------------------------------------------------------------------------

	Object3d * object3d = new Object3d();
	object3d->Initialize(object3dBasic);
	object3d->SetModel("teapot.obj");
	object3d->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));

	Object3d* object3d2 = new Object3d();
	object3d2->Initialize(object3dBasic);
	object3d2->SetModel("plane.obj");
	object3d2->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));

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
		if (ImGui::Button("Play")) {
			voiceHandle = audio->PlayWave(soundDataHandle, loopFlag, volume);
		}

		if (ImGui::Button("Stop")) {
			audio->StopWave(voiceHandle);
		}

		// set loop
		ImGui::Checkbox("Loop", &loopFlag);

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

	for (uint32_t i = 0; i < spriteNum; i++)
	{
		delete sprites[i];
	}



	winApp->Finalize();

	// ウィンドウクラスの解放
	delete winApp;

	return 0;
}


// 関数の定義-------------------------------------------------------------------------------------------------------------------

SoundData LoadWaveFile(const char* filename)
{
	//HRESULT result;

	std::ifstream file;
	// バイナリモードで開く
	file.open(filename, std::ios::binary);
	assert(file.is_open());

	// wavファイルのヘッダーを読み込む
	RiffHeader riff;
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));

	if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(false);
	}


	FormatChunk format = {};
	file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader));

	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
	{
		assert(false);
	}
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);


	ChunkHeader data;
	file.read(reinterpret_cast<char*>(&data), sizeof(data));

	if (strncmp(data.id, "JUNK ", 4) == 0)
	{
		file.seekg(data.size, std::ios::cur);
		file.read(reinterpret_cast<char*>(&data), sizeof(data));
	}

	if (strncmp(data.id, "data ", 4) != 0)
	{
		assert(false);
	}

	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	file.close();

	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

void SoundUnload(SoundData* soundData) {
	delete[] soundData->pBuffer;
	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

void SoundPlay(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT result;

	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	result = pSourceVoice->SubmitSourceBuffer(&buffer);
	result = pSourceVoice->Start();
}