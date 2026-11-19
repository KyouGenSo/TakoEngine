#include "TakoFramework.h"

#include "Audio.h"
#include "GameModule.h"
#include "Font.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "ModelManager.h"
#include "Renderer.h"
#include "LineRenderer.h"
#include "PostEffectManager.h"
#include "RenderPipeline.h"
#include "MaterialLibrary.h"
#include "GPUParticle.h"
#include "TransitionManager.h"
#include "FrameTimer.h"
#include "ShadowRenderer.h"
#include "DecalManager.h"
#include "Input.h"
#include "ProjectSettings.h"
#include "StringUtility.h"
#include "TypeRegistry.h"
#include "ColorFunc.h"
#include "ReflectionSelfTest.h"
#include "SceneSelfTest.h"

#include <chrono>
#include <cwchar>

#ifdef TAKO_EDITOR
#include "CrashHandler.h"
#include "DebugUIManager.h"
#endif

namespace Tako {

  void TakoFramework::Initialize()
  {
#ifdef TAKO_EDITOR
    // ゲームの DLL を読む前に入れる（スクリプトの static の初期化で落ちたときも書けるように）
    InstallCrashHandler();
    const bool hasCrashedLastTime = ConsumeCrashMarker();
#endif

    // 保存したファイルの "type" から物を作れるよう、最初に型を登録する
    RegisterTypes_Engine();
    GameModule::Load();
#ifdef _DEBUG
    RunReflectionSelfTest();
    RunColorSelfTest();
    RunFrameProfilerSelfTest();
#endif

#pragma region ウィンドウの初期化-------------------------------------------------------------------------------------------------------------------
    winApp_ = WinApp::GetInstance();

    // ウィンドウの生成に使うタイトル・サイズ・スタイルを先に読む。コードで設定済みの値を既定にし、JSON にある項目だけ上書きする
    ProjectSettings::WindowSettings& window = ProjectSettings::GetWindowSettings();
    window.productName = StringUtility::ConvertString(winApp_->GetWindowTitle());
    window.width       = WinApp::clientWidth;
    window.height      = WinApp::clientHeight;
    window.resizable   = winApp_->IsResizable();
    ProjectSettings::LoadWindow();
    winApp_->SetWindowTitle(StringUtility::ConvertString(window.productName));
    winApp_->SetWindowSize(window.width, window.height);
    winApp_->SetResizable(window.resizable);

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

    // エディタのプレビューがエフェクトを名前で引くので、エディタより先に登録する
    PostEffectManager::GetInstance()->Initialize(dx12_.get());

#ifdef TAKO_EDITOR
    // ImGuiManager の初期化（SrvManager のディスクリプタヒープを使用）
    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(winApp_, dx12_.get(), true);

    DebugUIManager::GetInstance()->Initialize();
    DebugUIManager::GetInstance()->SetEndFlagPtr(&endFlag_);
#endif

    Input::GetInstance()->Initialize(winApp_);

    Audio::GetInstance()->Initialize("resources/Sound/");

    TextureManager::GetInstance()->Initialize(dx12_.get(), "resources/Texture/");

    ModelManager::GetInstance()->Initialize(dx12_.get());

    // モデルの読み込みでマテリアルを作るので先に用意する
    Renderer::GetInstance()->Initialize(dx12_.get());

    LoadResources();

    FrameTimer::GetInstance()->Initialize();

    defaultCamera_ = std::make_unique<Camera>();
    defaultCamera_->SetRotate(Vector3(0.2f, 0.0f, 0.0f));
    defaultCamera_->SetTranslate(Vector3(0.0f, 9.0f, -34.0f));

    Renderer::GetInstance()->SetCamera(defaultCamera_.get());

    ShadowRenderer::GetInstance()->Initialize(dx12_.get());
    ShadowRenderer::GetInstance()->SetLight(Renderer::GetInstance()->GetLight());
    ShadowRenderer::GetInstance()->SetCamera(defaultCamera_.get());

    LineRenderer::GetInstance()->SetCamera(defaultCamera_.get());
    LineRenderer::GetInstance()->Initialize(dx12_.get());

    RenderPipeline::GetInstance()->Initialize(dx12_.get());

    // データファイルのシェーダーとポストエフェクトを登録する（Renderer と PostEffectManager の後）
    MaterialLibrary::GetInstance()->Initialize(dx12_.get());
    RenderPipeline::GetInstance()->LoadObjectPasses();

    DecalManager::GetInstance()->Initialize(dx12_.get());
    DecalManager::GetInstance()->SetCamera(defaultCamera_.get());

    TransitionManager::GetInstance()->Initialize();

    rendererOnResizeId_ = winApp_->RegisterOnResizeFunc(std::bind(&Renderer::OnResize, Renderer::GetInstance(), std::placeholders::_1));

    GPUParticle::GetInstance()->Initialize(dx12_.get(), defaultCamera_.get());

#pragma endregion

    // 設定の適用先（Audio・Shadow 等）が揃ってから読み込む
    ProjectSettings::Load(dx12_.get());

    // --profile を付けて起動すると、フレームの時間を測って画面に重ねて出す（エディタの無い Release で見る用）
    if (std::wcsstr(GetCommandLineW(), L"--profile")) {
      dx12_->GetProfiler().SetEnabled(true);
      profileOverlay_ = std::make_unique<ProfileOverlay>();
    }

    // Debug ではこの後に読む個人設定の Fullscreen が優先される
    if (ProjectSettings::GetWindowSettings().startFullscreen) {
      ToggleFullScreen();
    }

#ifdef _DEBUG
    RunSceneSelfTest();
#endif

#ifdef TAKO_EDITOR
    // 初期コンソールログ
    DebugUIManager::GetInstance()->AddLog("TakoEngine Initialized", DebugUIManager::LogType::Info);
    DebugUIManager::GetInstance()->AddLog("DirectX 12 Ready", DebugUIManager::LogType::Info);
    DebugUIManager::GetInstance()->AddLog("ImGui Docking Mode Enabled", DebugUIManager::LogType::Info);

    DebugUIManager::GetInstance()->LoadEditorSettings();
#endif

#ifdef TAKO_EDITOR
    if (hasCrashedLastTime) {
      // スクリプトが Play 中に落ちたのなら、Play で始めると同じ所でまた落ちる。Edit で始めて、直してから Play できるようにする
      SceneManager::GetInstance()->StartInEdit();
      DebugUIManager::GetInstance()->AddLog("The editor crashed last time (a dump is in Generated/Crashes). Started in Edit instead of Play", DebugUIManager::LogType::Warning);
    }
#endif

    // 個人設定の起動シーン（LoadEditorSettings が読む）が入った後に切り替える
    SceneManager::GetInstance()->ChangeToStartupScene();
  }

  void TakoFramework::Finalize()
  {
    // シーンマネージャーの終了処理（最初に実行）
    SceneManager::GetInstance()->Finalize();

    // 文字のバッファとマテリアルを持つので、Renderer と Font より先に消す
    profileOverlay_.reset();

    winApp_->UnregisterOnResizeFunc(rendererOnResizeId_);

    GPUParticle::GetInstance()->Finalize();

    // Initialize の逆順で終了処理を実行
    TransitionManager::GetInstance()->Finalize();

    MaterialLibrary::GetInstance()->Finalize();

    RenderPipeline::GetInstance()->Finalize();

    PostEffectManager::GetInstance()->Finalize();

    DecalManager::GetInstance()->Finalize();

    LineRenderer::GetInstance()->Finalize();

    ShadowRenderer::GetInstance()->Finalize();

    defaultCamera_.reset();

    FrameTimer::GetInstance()->Finalize();

    Renderer::GetInstance()->Finalize();

    ModelManager::GetInstance()->Finalize();

    Font::Finalize();

    TextureManager::GetInstance()->Finalize();

#ifdef TAKO_EDITOR
    // ImGui は SrvManager の SRV index を保持するため SrvManager より先に破棄する
    DebugUIManager::GetInstance()->Finalize();

    imguiManager_->Shutdown();
    imguiManager_.reset();
#endif

    // ゲームの型の物を持つ所（シーン・エディタ）を全部消した後で、DLL を外す
    GameModule::Unload();

    SrvManager::GetInstance()->Finalize();

    Audio::GetInstance()->Finalize();

    Input::GetInstance()->SetVibration(0.0f, 0.0f, 0.0f);
    Input::GetInstance()->Finalize();

    dx12_->Finalize();
    dx12_.reset();

    // WinApp（最初に初期化されたもの）
    winApp_->Finalize();
  }

  void TakoFramework::Update()
  {
    // 切り替えは SetWindowPos の中で ResizeBuffers まで同期的に走るので、前フレームの GPU 処理が終わっているここで行う
    if (Input::GetInstance()->TriggerKey(DIK_F11)) {
      ToggleFullScreen();
    }

    // ウィンドウメッセージの取得
    if (winApp_->ProcessMessage()) {
      endFlag_ = true;
      return;
    }

    FrameProfiler& profiler = dx12_->GetProfiler();

#ifdef TAKO_EDITOR
    const uint32_t hotReloadScope = profiler.BeginCpuScope("Hot Reload");
    // GPU は前フレームを描き終えているので、ここで PSO を作り直してよい
    dx12_->GetShaderLibrary().ReloadChangedShaders();
    MaterialLibrary::GetInstance()->Update();
    // シーンを消して作り直すので、このフレームの更新が始まる前に行う
    DebugUIManager::GetInstance()->UpdateGameModule();
    profiler.EndScope(hotReloadScope);
#endif

    // 前フレームの GPU 処理は EndDraw で完了済みなので、ここなら RT を作り直せる。
    // 自前の RT を持つエフェクトもリサイズ通知で作り直すため、OnWindowResize を直接呼ばず通知経由にする
    if (dx12_->ConsumeRenderTargetRebuildRequest()) {
      winApp_->NotifyResize(WinApp::clientWidth, WinApp::clientHeight);
    }

    defaultCamera_->Update();

    FrameTimer::GetInstance()->Update();

    Input::GetInstance()->Update();

    Audio::GetInstance()->Update();

    Font::BeginFrame();

    // Actor の生成・削除と Play / Stop は、一時停止中もここで行う（前のフレームの GPU 処理は EndDraw で終わっている）
    const uint32_t spawnScope = profiler.BeginCpuScope("Spawn / Destroy");
    SceneManager::GetInstance()->UpdateAlways();
    profiler.EndScope(spawnScope);

    // 一時停止中はシーン更新だけ止める。以降の更新は deltaTime が 0 になるため止まって見える
    // (GPUParticle/Decal は Debug Viewport 用リングのリセットを含むので毎フレーム呼ぶ)
    if (!FrameTimer::GetInstance()->IsFrozen()) {
      SceneManager::GetInstance()->Update();
    }

    const uint32_t particleScope = profiler.BeginCpuScope("Particles");
    GPUParticle::GetInstance()->Update();
    profiler.EndScope(particleScope);

    const uint32_t decalScope = profiler.BeginCpuScope("Decals");
    DecalManager::GetInstance()->UpdateAll();
    profiler.EndScope(decalScope);

    PostEffectManager::GetInstance()->Update(FrameTimer::GetInstance()->GetDeltaTime());

#ifdef TAKO_EDITOR
    const uint32_t editorScope = profiler.BeginCpuScope("Editor");
    DebugUIManager::GetInstance()->Update();
    profiler.EndScope(editorScope);
#endif

    LineRenderer::GetInstance()->Update();

    Renderer::GetInstance()->Update();

    ShadowRenderer::GetInstance()->Update();

    if (profileOverlay_) {
      profileOverlay_->Update(dx12_->GetProfiler());
    }

  }

  void TakoFramework::Draw()
  {
    SrvManager::GetInstance()->BeginDraw();

    FrameProfiler& profiler = dx12_->GetProfiler();

    // 描画物を全て登録してから、まとめて描く。ポストエフェクトの前後は描画キューで分かれる（kQueueOverlay 以上が後）
    Renderer* renderer = Renderer::GetInstance();
    renderer->BeginFrame();
    // スキニングの計算（コンピュート）は、描画物の登録の中で積まれる
    const uint32_t submitScope = profiler.BeginScope("Submit");
    renderer->BeginSubmit(renderer->GetSceneList());
    SceneManager::GetInstance()->Draw();
#ifdef TAKO_EDITOR
    // シーンの HUD と同じ RT へ、UI Editor のプレビューを重ねる
    DebugUIManager::GetInstance()->DrawUILayoutPreviewPass();
#endif
    TransitionManager::GetInstance()->Draw();
    if (profileOverlay_) {
      profileOverlay_->Draw();
    }
    renderer->EndSubmit();
    profiler.EndScope(submitScope);

    RenderPipeline::GetInstance()->Render();

#ifdef TAKO_EDITOR
    // 本編で積んだ線分を別視点でも描くため Reset より前に行う
    const uint32_t debugViewportScope = profiler.BeginScope("Debug Viewport");
    DebugUIManager::GetInstance()->DrawDebugViewportPass();
    profiler.EndScope(debugViewportScope);
#endif

    LineRenderer::GetInstance()->Reset();

#ifdef TAKO_EDITOR
    // 各エディターのプレビューをオフスクリーン RT へ描画
    const uint32_t previewScope = profiler.BeginScope("Editor Previews");
    DebugUIManager::GetInstance()->DrawPrimitivePreviewPass();
    DebugUIManager::GetInstance()->DrawParticlePreviewPass();
    DebugUIManager::GetInstance()->DrawAnimationPreviewPass();
    DebugUIManager::GetInstance()->DrawMaterialPreviewPass();
    profiler.EndScope(previewScope);
#endif

    /// ============================================= ///
    /// ---------最終結果をスワップチェーンに描画---------///
    /// ============================================= ///
    bool isDrawToSwapChain = true;

#ifdef TAKO_EDITOR
    isDrawToSwapChain = !DebugUIManager::GetInstance()->IsWindowVisible(DebugUIManager::Window::GameViewport);
#endif

    const uint32_t presentScope = profiler.BeginScope("Present");
    RenderPipeline::GetInstance()->Present(isDrawToSwapChain);
    profiler.EndScope(presentScope);


    /// ========================================= ///
    ///-------------------ImGui-------------------///
    /// ========================================= ///
#ifdef TAKO_EDITOR

    const uint32_t imguiScope = profiler.BeginScope("ImGui");
    imguiManager_->Begin();

    DebugUIManager::GetInstance()->Draw();

    LineRenderer::GetInstance()->ImGui();

    imguiManager_->End();

    imguiManager_->Draw();
    profiler.EndScope(imguiScope);

    // ImGui はコマンドリストを直接操作するので、記憶しているパイプラインと束縛を捨てる
    dx12_->GetContext().InvalidateState();
#endif


    dx12_->EndDraw();

    // EndDraw で GPU 完了を待った後なので、このフレームまでに積んだアップロードは転送済み
    TextureManager::GetInstance()->EndFrame();
  }

  void TakoFramework::Run()
  {
    Initialize();

    while (true) {
      const auto updateStart = std::chrono::steady_clock::now();
      Update();

      if (GetEndFlag()) {
        break;
      }

      const auto drawStart = std::chrono::steady_clock::now();
      Draw();
      const auto drawEnd = std::chrono::steady_clock::now();
      dx12_->GetProfiler().AddCpuTimes(std::chrono::duration<float, std::milli>(drawStart - updateStart).count(),
                                       std::chrono::duration<float, std::milli>(drawEnd - drawStart).count());
    }

    Finalize();
  }

  void TakoFramework::ToggleFullScreen()
  {
    winApp_->ToggleFullScreen();
  }

  void TakoFramework::OnWindowResize(uint32_t width, uint32_t height)
  {
    // GPU の処理を待機
    //dx12_->WaitForGPU();

    // バッファのリサイズ
    dx12_->ResizeBuffers(width, height);

    // シーン色・最終結果の RT の再作成
    RenderPipeline::GetInstance()->OnResize();

    // デカール深度 SRV の再作成
    DecalManager::GetInstance()->OnResize();

    // パーティクル深度 SRV の再作成
    GPUParticle::GetInstance()->OnResize();

    // カメラのアスペクト比を更新
    defaultCamera_->UpdateProjectionMatrix();

#ifdef TAKO_EDITOR
    imguiManager_->OnWindowResize();
#endif
  }

  void TakoFramework::LoadResources()
  {
    TextureManager* tm = TextureManager::GetInstance();
    tm->LoadEngineDefault("white.dds");
    tm->LoadEngineDefault("black.dds");
    tm->LoadEngineDefault("white.png");
    tm->LoadEngineDefault("black.png");
    tm->LoadEngineDefault("circle.dds");
    tm->LoadEngineDefault("circle2.dds");
    tm->LoadEngineDefault("spark.dds");
    tm->LoadEngineDefault("ring.png");
    tm->LoadEngineDefault("my_skybox.dds");
    tm->LoadEngineDefault("noise0.png");

    ModelManager* mm = ModelManager::GetInstance();
    mm->LoadEngineModel("sphere.gltf");
    mm->LoadEngineModel("white_cube.gltf");
  }

} // namespace Tako

