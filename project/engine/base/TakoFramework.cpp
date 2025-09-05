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
#include "Transition.h"
#include "FrameTimer.h"
#include "ShadowRenderer.h"

void TakoFramework::Initialize()
{
#pragma region ウィンドウの初期化-------------------------------------------------------------------------------------------------------------------
  winApp_ = WinApp::GetInstance();
	winApp_->Initialize();
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
#endif

	TextureManager::GetInstance()->Initialize(dx12_, "resources/Texture/");

	ModelManager::GetInstance()->Initialize(dx12_);

	Object3dBasic::GetInstance()->Initialize(dx12_);

	SpriteBasic::GetInstance()->Initialize(dx12_);

  DebugCamera::GetInstance()->Initialize();

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

  Transition::GetInstance()->Initialize();

#pragma endregion
}

void TakoFramework::Finalize()
{
  // シーンマネージャーの終了処理（最初に実行）
  SceneManager::GetInstance()->Finalize();

  // Initializeの逆順で終了処理を実行
  // Transition
  Transition::GetInstance()->Finalize();

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

  // DebugCamera
  DebugCamera::GetInstance()->Finalize();

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

	//	Draw2Dの更新
	Draw2D::GetInstance()->Update();

	// シーンマネージャーの更新
	SceneManager::GetInstance()->Update();

	// Object3dBasicの更新
	Object3dBasic::GetInstance()->Update();

  // ShadowRendererの更新
  ShadowRenderer::GetInstance()->Update();

}

void TakoFramework::Draw()
{
#ifdef _DEBUG
  ImGui::Begin("Option");
  // buttonでFPSの表示を切り替え
  if (ImGui::Button("Display FPS"))
  {
    FPSWindowVisible = !FPSWindowVisible;
  }
  ImGui::SameLine();
  if (ImGui::Button("PostEffect Option"))
  {
    PostEffectWindowVisible = !PostEffectWindowVisible;
  }
  ImGui::SameLine();
  if (ImGui::Button("Game Viewport"))
  {
    GameViewportWindowVisible = !GameViewportWindowVisible;
  }

  ImGui::End();

  // fpsの表示
  if (FPSWindowVisible)
  {
    ImGui::Begin("FPS", &FPSWindowVisible);
    ImGui::ProgressBar(FrameTimer::GetInstance()->GetFPS() / 60.0f, ImVec2(0.0f, 0.0f), "");
    ImGui::SameLine();
    ImGui::Text("FPS : %.0f", FrameTimer::GetInstance()->GetFPS());
    ImGui::End();
  }


  // PostEffectのパラメータ調整
  if (PostEffectWindowVisible) {
    PostEffectManager::GetInstance()->DrawImgui();
  }

  // ゲームビューポートウィンドウの表示
  if (GameViewportWindowVisible) {
    ImGui::Begin("Game Viewport", &GameViewportWindowVisible);

    // ウィンドウの利用可能サイズを取得
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    // クライアント領域のアスペクト比を計算
    float aspectRatio = static_cast<float>(WinApp::clientWidth) / static_cast<float>(WinApp::clientHeight);

    // アスペクト比を維持したサイズを計算
    ImVec2 imageSize;
    float availableAspect = availableSize.x / availableSize.y;

    if (availableAspect > aspectRatio) {
      // ウィンドウが横長の場合、高さに合わせる
      imageSize.y = availableSize.y;
      imageSize.x = imageSize.y * aspectRatio;
    } else {
      // ウィンドウが縦長の場合、幅に合わせる
      imageSize.x = availableSize.x;
      imageSize.y = imageSize.x / aspectRatio;
    }

    // 画像を中央に配置するためのカーソル位置を計算
    ImVec2 cursorPos = ImGui::GetCursorPos();
    cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
    cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
    ImGui::SetCursorPos(cursorPos);

    // PostEffectManagerから直接SRVインデックスを取得してゲーム画面を表示
    uint32_t srvIndex = PostEffectManager::GetInstance()->GetFinalResultSrvIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
    ImGui::Image((ImTextureID)gpuHandle.ptr, imageSize);

    ImGui::End();
  }
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

  // 画面サイズを取得
  uint32_t width = WinApp::clientWidth;
  uint32_t height = WinApp::clientHeight;

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
