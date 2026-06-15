#include "TakoFramework.h"

#include "Audio.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "ModelManager.h"
#include "Object3dBasic.h"
#include "SpriteBasic.h"
#include "Draw2D.h"
#include "PostEffectManager.h"
#include "GPUParticle.h"
#include "TransitionManager.h"
#include "FrameTimer.h"
#include "ShadowRenderer.h"
#include "DecalManager.h"
#include "Input.h"

#ifdef _DEBUG
#include "DebugCamera.h"
#include "DebugUIManager.h"
#endif

namespace Tako {

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
    dx12_ = std::make_unique<DX12Basic>();
    dx12_->Initialize(winApp_);

    // SrvManager を先に初期化（ImGuiManager が使用するため）
    SrvManager::GetInstance()->Initialize(dx12_.get());

#ifdef _DEBUG
    // ImGuiManager の初期化（SrvManager のディスクリプタヒープを使用）
    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(winApp_, dx12_.get(), true);

    // DebugUIManager の初期化
    DebugUIManager::GetInstance()->Initialize();
    DebugUIManager::GetInstance()->SetEndFlagPtr(&endFlag_);
    DebugUIManager::GetInstance()->SetDebugFlagPtr(&isDebug_);

    DebugCamera::GetInstance()->Initialize();
#endif

    Input::GetInstance()->Initialize(winApp_);

    Audio::GetInstance()->Initialize("resources/Sound/");

    TextureManager::GetInstance()->Initialize(dx12_.get(), "resources/Texture/");

    ModelManager::GetInstance()->Initialize(dx12_.get());

    LoadResources();

    Object3dBasic::GetInstance()->Initialize(dx12_.get());

    SpriteBasic::GetInstance()->Initialize(dx12_.get());

    FrameTimer::GetInstance()->Initialize();

    // デフォルトカメラを生成
    defaultCamera_ = std::make_unique<Camera>();
    defaultCamera_->SetRotate(Vector3(0.2f, 0.0f, 0.0f));
    defaultCamera_->SetTranslate(Vector3(0.0f, 9.0f, -34.0f));

    // デフォルトカメラを設定
    Object3dBasic::GetInstance()->SetCamera(defaultCamera_.get());

    // ShadowRenderer の初期化
    ShadowRenderer::GetInstance()->Initialize(dx12_.get());
    ShadowRenderer::GetInstance()->SetLight(Object3dBasic::GetInstance()->GetLight());
    ShadowRenderer::GetInstance()->SetCamera(defaultCamera_.get());

    Draw2D::GetInstance()->SetCamera(defaultCamera_.get());
    Draw2D::GetInstance()->Initialize(dx12_.get());

    PostEffectManager::GetInstance()->Initialize(dx12_.get());

    PostEffectManager::GetInstance()->SetCamera(defaultCamera_.get());

    // DecalManager の初期化（PostEffectManager の後、深度SRVを作成するため）
    DecalManager::GetInstance()->Initialize(dx12_.get());
    DecalManager::GetInstance()->SetCamera(defaultCamera_.get());

    TransitionManager::GetInstance()->Initialize();

    // SpriteBasic のリサイズコールバック関数登録
    spriteBasicOnresizeId_ = winApp_->RegisterOnResizeFunc(std::bind(&SpriteBasic::OnResize, SpriteBasic::GetInstance(), std::placeholders::_1));

    // GPU パーティクルの初期化
    GPUParticle::GetInstance()->Initialize(dx12_.get(), defaultCamera_.get());

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

    winApp_->UnregisterOnResizeFunc(spriteBasicOnresizeId_);

    // GPU パーティクルの解放
    GPUParticle::GetInstance()->Finalize();

    // Initialize の逆順で終了処理を実行
    // TransitionManager
    TransitionManager::GetInstance()->Finalize();

    // PostEffectManager
    PostEffectManager::GetInstance()->Finalize();

    // DecalManager
    DecalManager::GetInstance()->Finalize();

    // Draw2D
    Draw2D::GetInstance()->Finalize();

    // ShadowRenderer
    ShadowRenderer::GetInstance()->Finalize();

    // defaultCamera は unique_ptr で自動解放
    defaultCamera_.reset();

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

    // SRV マネージャー
    SrvManager::GetInstance()->Finalize();

    // Audio の解放
    Audio::GetInstance()->Finalize();

    // 入力クラスの解放
    Input::GetInstance()->SetVibration(0.0f, 0.0f, 0.0f);
    Input::GetInstance()->Finalize();

#ifdef _DEBUG
    // DebugUIManager の終了処理
    DebugUIManager::GetInstance()->Finalize();

    // ImGuiManager の終了処理
    imguiManager_->Shutdown();
    imguiManager_.reset();
#endif

    // DX12の終了処理
    dx12_->Finalize();
    dx12_.reset();

    // sceneFactory は unique_ptr で自動解放
    sceneFactory_.reset();

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

    // カメラの更新
    defaultCamera_->Update();

    // フレームタイマーの更新
    FrameTimer::GetInstance()->Update();

    // 入力情報の更新
    Input::GetInstance()->Update();

    //　サウンドの更新
    Audio::GetInstance()->Update();

    // シーンマネージャーの更新
    SceneManager::GetInstance()->Update();

    // GPU パーティクルの更新
    GPUParticle::GetInstance()->Update();

    // デコールの更新
    DecalManager::GetInstance()->UpdateAll();

    // 一時エフェクトの更新
    PostEffectManager::GetInstance()->Update(FrameTimer::GetInstance()->GetDeltaTime());

#ifdef _DEBUG
    if (Input::GetInstance()->TriggerKey(DIK_F1)) {
      isDebug_ = !isDebug_;
      Object3dBasic::GetInstance()->SetDebug(isDebug_);
      Draw2D::GetInstance()->SetDebug(isDebug_);
      GPUParticle::GetInstance()->SetIsDebug(isDebug_);
    }

    if (isDebug_) {
      DebugCamera::GetInstance()->Update();
    }

    DebugUIManager::GetInstance()->Update();
#endif

    //	Draw2D の更新
    Draw2D::GetInstance()->Update();

    // Object3dBasic の更新
    Object3dBasic::GetInstance()->Update();

    // ShadowRenderer の更新
    ShadowRenderer::GetInstance()->Update();

  }

  void TakoFramework::Draw()
  {
    /// ============================================= ///
    /// ------------------シーン描画-------------------///
    /// ============================================= ///

    //ポストエフェクト適用対象のレンダーテクスチャを描画先に設定
    dx12_->SetEffectRenderTexture();

    // テクスチャ用の srv ヒープの設定
    SrvManager::GetInstance()->BeginDraw();

    SceneManager::GetInstance()->Draw();

    DecalManager::GetInstance()->DrawAll();

#ifdef _DEBUG
    DecalManager::GetInstance()->DrawAllDebug();
#endif

    GPUParticle::GetInstance()->Draw();

    Draw2D::GetInstance()->Draw();

    /// ===================================================== ///
    /// ------------------ポストエフェクト描画-------------------///
    /// ===================================================== ///

    // ポストエフェクトの描画
    PostEffectManager::GetInstance()->Draw();

    /// ===================================================== ///
    /// ------------ポストエフェクト非適用対象の描画---------------///
    /// ===================================================== ///
    // ポストエフェクト非適用対象のレンダーテクスチャを描画先に設定
    dx12_->SetNonEffectRenderTexture();

    // シーンの描画
    SceneManager::GetInstance()->DrawWithoutEffect();

    TransitionManager::GetInstance()->Draw();

    Draw2D::GetInstance()->Reset();

    /// ============================================= ///
    /// ---------最終結果をスワップチェーンに描画---------///
    /// ============================================= ///
    bool isDrawToSwapChain = true;

#ifdef _DEBUG
    isDrawToSwapChain = !DebugUIManager::GetInstance()->IsWindowVisible("GameViewport");
#endif

    PostEffectManager::GetInstance()->DrawFinalResult(isDrawToSwapChain);


    /// ========================================= ///
    ///-------------------ImGui-------------------///
    /// ========================================= ///
#ifdef _DEBUG

    imguiManager_->Begin();

    // デバッグ UI の描画
    DebugUIManager::GetInstance()->Draw();

    Draw2D::GetInstance()->ImGui();

    imguiManager_->End();

    //imgui の描画
    imguiManager_->Draw();
#endif


    // 描画後の処理
    dx12_->EndDraw();
  }

  void TakoFramework::Run()
  {
    Initialize();

    while (true) {
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
    // GPU の処理を待機
    //dx12_->WaitForGPU();

    // バッファのリサイズ
    dx12_->ResizeBuffers(width, height);

    // レンダーテクスチャの再作成（PostEffect 用）
    PostEffectManager::GetInstance()->RecreateRenderTexture();

    // デカール深度 SRV の再作成
    DecalManager::GetInstance()->OnResize();

    // パーティクル深度 SRV の再作成
    GPUParticle::GetInstance()->OnResize();

    // カメラのアスペクト比を更新
    defaultCamera_->UpdateProjectionMatrix();

#ifdef _DEBUG
    imguiManager_->OnWindowResize();
#endif
  }

  void TakoFramework::LoadResources()
  {
    TextureManager* tm = TextureManager::GetInstance();
    tm->LoadEngineDefault("white.dds");
    tm->LoadEngineDefault("black.dds");
    tm->LoadEngineDefault("circle.dds");
    tm->LoadEngineDefault("circle2.dds");
    tm->LoadEngineDefault("spark.dds");
    tm->LoadEngineDefault("ring.png");
    tm->LoadEngineDefault("my_skybox.dds");
    tm->LoadEngineDefault("black.dds");
    tm->LoadEngineDefault("noise0.png");

    ModelManager* mm = ModelManager::GetInstance();
    mm->LoadEngineModel("sphere.gltf");
    mm->LoadEngineModel("white_cube.gltf");
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

} // namespace Tako

