#include "TakoFramework.h"

void TakoFramework::Initialize()
{
#pragma region ウィンドウの初期化-------------------------------------------------------------------------------------------------------------------
	winApp_ = new WinApp();
	winApp_->Initialize();
#pragma endregion


#pragma region 基盤システムの初期化-------------------------------------------------------------------------------------------------------------------
	dx12_ = new DX12Basic();
	dx12_->Initialize(winApp_);

#ifdef _DEBUG
	imguiManager_ = new ImGuiManager();
	imguiManager_->Initialize(winApp_, dx12_);
#endif

	srvManager_ = new SrvManager();
	srvManager_->Initialize(dx12_);

	TextureManager::GetInstance()->Initialize(dx12_, srvManager_);

	ModelManager::GetInstance()->Initialize(dx12_);

	object3dBasic_ = new Object3dBasic();
	object3dBasic_->Initialize(dx12_);

	spriteBasic_ = new SpriteBasic();
	spriteBasic_->Initialize(dx12_);

	defaultCamera_ = new Camera();
	defaultCamera_->SetRotate(Vector3(0.3f, 0.0f, 0.0f));
	defaultCamera_->SetTranslate(Vector3(0.0f, 4.0f, -10.0f));
	// デフォルトカメラを設定
	object3dBasic_->SetDefaultCamera(defaultCamera_);
#pragma endregion

}

void TakoFramework::Finalize()
{
	// ModelManagerの終了処理
	ModelManager::GetInstance()->Finalize();

	// TextureManagerの終了処理
	TextureManager::GetInstance()->Finalize();

#ifdef _DEBUG
	// ImGuiManagerの終了処理
	imguiManager_->Shutdown();
	delete imguiManager_;
#endif

	// DX12の終了処理
	dx12_->Finalize();

	// pointerの解放
	delete dx12_;
	delete spriteBasic_;
	delete defaultCamera_;
	delete object3dBasic_;
	delete srvManager_;

	winApp_->Finalize();

	// ウィンドウクラスの解放
	delete winApp_;
}

void TakoFramework::Update()
{
	// ウィンドウメッセージの取得
	if (winApp_->ProcessMessage()) {
		endFlag_ = true;
		return;
	}
}

void TakoFramework::Run()
{
	Initialize();

	while (true)
	{
		Update();

		if (GetEndFlag()) {
			break;
		}

		Draw();
	}

	Finalize();
}
