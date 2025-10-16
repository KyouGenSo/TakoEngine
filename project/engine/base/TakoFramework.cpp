#include "TakoFramework.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "ModelManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include "Model.h"
#include "Draw2D.h"
#include "PostEffectManager.h"
#include "DebugCamera.h"
#include "TransitionManager.h"
#include "FrameTimer.h"
#include "ShadowRenderer.h"
#include "Input.h"
#include "CollisionManager.h"
#include "Light.h"
#include <algorithm>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "GPUParticle.h"

void TakoFramework::Initialize()
{

#pragma region ウィンドウの初期化-------------------------------------------------------------------------------------------------------------------
  winApp_ = WinApp::GetInstance();
	winApp_->Initialize();

	// ウィンドウリサイズ時のコールバックを登録
	winApp_->RegisterOnResizeFunc([this](Vector2 size) {
		OnWindowResize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
	});
#pragma endregion


#pragma region 基盤システムの初期化-------------------------------------------------------------------------------------------------------------------
	dx12_ = new DX12Basic();
	dx12_->Initialize(winApp_);

	// SrvManagerを先に初期化（ImGuiManagerが使用するため）
	SrvManager::GetInstance()->Initialize(dx12_);

#ifdef _DEBUG
	// ImGuiManagerの初期化（SrvManagerのディスクリプタヒープを使用）
	imguiManager_ = new ImGuiManager();
  imguiManager_->Initialize(winApp_, dx12_, true);
  
  // DebugUIManagerの初期化
  DebugUIManager::GetInstance()->Initialize();
  DebugUIManager::GetInstance()->SetEndFlagPtr(&endFlag_);
  DebugUIManager::GetInstance()->SetDebugFlagPtr(&isDebug_);

  DebugCamera::GetInstance()->Initialize();
#endif

	TextureManager::GetInstance()->Initialize(dx12_, "resources/Texture/");

	ModelManager::GetInstance()->Initialize(dx12_);

	Object3dBasic::GetInstance()->Initialize(dx12_);

	SpriteBasic::GetInstance()->Initialize(dx12_);

  FrameTimer::GetInstance()->Initialize();

  // デフォルトカメラを生成
	defaultCamera_ = new Camera();
	defaultCamera_->SetRotate(Vector3(0.2f, 0.0f, 0.0f));
	defaultCamera_->SetTranslate(Vector3(0.0f, 9.0f, -34.0f));

	// デフォルトカメラを設定
	Object3dBasic::GetInstance()->SetCamera(defaultCamera_);

  // ShadowRendererの初期化
  ShadowRenderer::GetInstance()->Initialize(dx12_);
  ShadowRenderer::GetInstance()->SetLight(Object3dBasic::GetInstance()->GetLight());
  ShadowRenderer::GetInstance()->SetCamera(defaultCamera_);

  Draw2D::GetInstance()->SetCamera(defaultCamera_);
  Draw2D::GetInstance()->Initialize(dx12_);

  TextureManager::GetInstance()->LoadTexture("black.png");
  TextureManager::GetInstance()->LoadTexture("noise0.png");

  PostEffectManager::GetInstance()->Initialize(dx12_);

  PostEffectManager::GetInstance()->SetCamera(defaultCamera_);

  TransitionManager::GetInstance()->Initialize();

#pragma endregion

#ifdef _DEBUG
  // 初期コンソールログ
  DebugUIManager::GetInstance()->AddLog("TakoEngine Initialized", DebugUIManager::LogType::Info);
  DebugUIManager::GetInstance()->AddLog("DirectX 12 Ready", DebugUIManager::LogType::Info);
  DebugUIManager::GetInstance()->AddLog("ImGui Docking Mode Enabled", DebugUIManager::LogType::Info);
#endif
}

void TakoFramework::Finalize()
{
  // シーンマネージャーの終了処理（最初に実行）
  SceneManager::GetInstance()->Finalize();

  // Initializeの逆順で終了処理を実行
  // TransitionManager
  TransitionManager::GetInstance()->Finalize();

  // PostEffectManager
  PostEffectManager::GetInstance()->Finalize();

  // Draw2D
  Draw2D::GetInstance()->Finalize();

  // ShadowRenderer
  ShadowRenderer::GetInstance()->Finalize();

  // defaultCameraの削除
  delete defaultCamera_;

  // FrameTimer
  FrameTimer::GetInstance()->Finalize();

#ifdef _DEBUG
  // DebugCamera
  DebugCamera::GetInstance()->Finalize();
#endif

  // SpriteBasic
  SpriteBasic::GetInstance()->Finalize();

  // Object3dBasic
  Object3dBasic::GetInstance()->Finalize();

  // ModelManager
  ModelManager::GetInstance()->Finalize();

  // TextureManager
  TextureManager::GetInstance()->Finalize();

  // SRVマネージャー
  SrvManager::GetInstance()->Finalize();

#ifdef _DEBUG
  // DebugUIManagerの終了処理
  DebugUIManager::GetInstance()->Finalize();
  
  // ImGuiManagerの終了処理
  imguiManager_->Shutdown();
  delete imguiManager_;
#endif

  // DX12の終了処理
  dx12_->Finalize();
  delete dx12_;

  // その他のポインタ解放
  delete sceneFactory_;

  // WinApp（最初に初期化されたもの）
  winApp_->Finalize();
}

void TakoFramework::Update()
{
	// ウィンドウメッセージの取得
	if (winApp_->ProcessMessage()) {
		endFlag_ = true;
		return;
	}

  // フレームタイマーの更新
  FrameTimer::GetInstance()->Update();

  // シーンマネージャーの更新
  SceneManager::GetInstance()->Update();

#ifdef _DEBUG
  if (Input::GetInstance()->TriggerKey(DIK_F1))
  {
    isDebug_ = !isDebug_;
    Object3dBasic::GetInstance()->SetDebug(isDebug_);
    Draw2D::GetInstance()->SetDebug(isDebug_);
    GPUParticle::GetInstance()->SetIsDebug(isDebug_);
  }

  if (isDebug_)
  {
    DebugCamera::GetInstance()->Update();
  }

  DebugUIManager::GetInstance()->Update();
#endif

	//	Draw2Dの更新
	Draw2D::GetInstance()->Update();

	// Object3dBasicの更新
	Object3dBasic::GetInstance()->Update();

  // ShadowRendererの更新
  ShadowRenderer::GetInstance()->Update();

}

void TakoFramework::Draw()
{
#ifdef _DEBUG
  // デバッグUIの描画
  DebugUIManager::GetInstance()->Draw();
#endif
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

void TakoFramework::ToggleFullScreen()
{
  // ウィンドウの状態を切り替え
  winApp_->ToggleFullScreen();

  // リサイズ処理を実行
  OnWindowResize(WinApp::clientWidth, WinApp::clientHeight);
}

void TakoFramework::OnWindowResize(uint32_t width, uint32_t height)
{
  // GPUの処理を待機
  dx12_->WaitForGPU();

  // バッファのリサイズ
  dx12_->ResizeBuffers(width, height);

  // レンダーテクスチャの再作成（PostEffect用）
  PostEffectManager::GetInstance()->RecreateRenderTexture();

  // カメラのアスペクト比を更新
  defaultCamera_->UpdateProjectionMatrix();

#ifdef _DEBUG
  imguiManager_->OnWindowResize();
#endif
}

#ifdef _DEBUG
void TakoFramework::SetIsDebug(bool value)
{
  isDebug_ = value;
  // 各コンポーネントのデバッグモードも同時に設定
  Object3dBasic::GetInstance()->SetDebug(isDebug_);
  Draw2D::GetInstance()->SetDebug(isDebug_);
  GPUParticle::GetInstance()->SetIsDebug(isDebug_);
}
#endif

