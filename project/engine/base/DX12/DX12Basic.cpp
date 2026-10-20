#include "DX12Basic.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <thread>
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "StringUtility.h"
#include "RtvManager.h"
#include "DsvManager.h"

#ifdef _DEBUG
#include"imgui.h"
#include"imgui_impl_win32.h"
#include"imgui_impl_dx12.h"
#endif

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxguid.lib")

namespace Tako {

  DX12Basic::~DX12Basic()
  {

  }

  void DX12Basic::Initialize(WinApp* winApp)
  {
    assert(winApp);
    this->winApp_ = winApp;

    InitFPSLimiter();

    InitDevice();

    InitCommand();

    CreateSwapChain();

    CreateDepthStencilResource();

    InitDescriptorHeap();

    // レンダーテクスチャの初期化
    //InitRenderTexture();

    InitSwapChainRTV();

    InitDSV();

    InitFence();

    InitViewport();

    InitScissorRect();

    shaderLibrary_.Initialize();

  }

  void DX12Basic::Finalize()
  {
    // コマンド完了まで待機
    fenceValue_++;
    // コマンドキューに Fence のシグナルを行う
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
      fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
      WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // スワップチェーン用 RTV とメイン深度の DSV を返却
    for (UINT i = 0; i < kRtvHandleCount; ++i) {
      RtvManager::GetInstance()->Free(swapChainRtvIndices_[i]);
      swapChainRtvIndices_[i] = RtvManager::kInvalidIndex;
    }
    DsvManager::GetInstance()->Free(mainDsvIndex_);
    mainDsvIndex_ = DsvManager::kInvalidIndex;
    DsvManager::GetInstance()->Free(nonEffectDsvIndex_);
    nonEffectDsvIndex_ = DsvManager::kInvalidIndex;

    // RTV/DSV マネージャーの終了処理（内部でリーク検査）
    RtvManager::GetInstance()->Finalize();
    DsvManager::GetInstance()->Finalize();

    // イベントの破棄
    CloseHandle(fenceEvent_);
  }

  void DX12Basic::SetSwapChain()
  {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    context_.Transition(swapChainResources_[backBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
    context_.Transition(depthStencilResource_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    context_.SetRenderTarget(&rtvHandle_[backBufferIndex], kColorFormat, nullptr, DXGI_FORMAT_UNKNOWN);

    float clearColor[] = { 0.17f, 0.17f, 0.17f, 1.0f };
    context_.GetCommandList()->ClearRenderTargetView(rtvHandle_[backBufferIndex], clearColor, 0, nullptr);

    SetViewPort();
  }

  void DX12Basic::EndDraw()
  {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    context_.Transition(swapChainResources_[backBufferIndex], D3D12_RESOURCE_STATE_PRESENT);
    context_.Transition(depthStencilResource_, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    context_.Close();

    ID3D12CommandList* commandLists[] = { context_.GetCommandList() };
    commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);

    WaitForGPU();

    UpdateFPSLimiter();

    swapChain_->Present(vsync_ ? 1 : 0, (!vsync_ && isTearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0);

    // GPU の完了を待ったので、このフレームのコマンドと一時定数はもう読まれない
    context_.Reset();
    uploadAllocator_.Reset();
  }

  void DX12Basic::WaitForGPU()
  {
    // コマンド完了まで待機
    fenceValue_++;
    // コマンドキューに Fence のシグナルを行う
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
      fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
      WaitForSingleObject(fenceEvent_, INFINITE);
    }
  }

  void DX12Basic::InitDevice()
  {
    HRESULT hr;

    //-----------------------------------------------------デバッグレイヤーの設定-----------------------------------------------------
#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
      //デバッグレイヤーを有効にする
      debugController->EnableDebugLayer();
      debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    //------------------------------------------------------DXGI ファクトリの生成------------------------------------------------------
    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    //-----------------------------------------------------------アダプターの列挙-----------------------------------------------------------
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    for (UINT adapterIndex = 0; dxgiFactory_->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
      //アダプターの情報を取得
      DXGI_ADAPTER_DESC3 desc;
      hr = useAdapter->GetDesc3(&desc);
      assert(SUCCEEDED(hr));

      if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "Use Adapter:" + StringUtility::ConvertString(desc.Description),
          DebugUIManager::LogType::Info);
#endif

        break;
      }
      useAdapter = nullptr;
    }
    assert(useAdapter != nullptr);

    //---------------------------------------------------------デバイスの生成---------------------------------------------------------
    // 機能レベルとログ出力用文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_12_2,
      D3D_FEATURE_LEVEL_12_1,
      D3D_FEATURE_LEVEL_12_0
    };
    const char* featureLevelNames[] = {
      "12.2",
      "12.1",
      "12.0"
    };

    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
      hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
      if (SUCCEEDED(hr)) {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "Feature Level: " + std::string(featureLevelNames[i]),
          DebugUIManager::LogType::Info);
#endif

        break;
      }
    }
    assert(device_ != nullptr);

#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("D3D12Device Created", DebugUIManager::LogType::Info);
#endif



    //-----------------------------------------------------エラーチェック-----------------------------------------------------
#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
      // デバッグレイヤーの設定
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

      D3D12_MESSAGE_ID denyIds[] = {
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        (D3D12_MESSAGE_ID)1424, // FENCE_ZERO_WAIT - ImGui の内部 Fence 処理で発生する警告を除外
      };

      D3D12_MESSAGE_SEVERITY severities[] = {
        D3D12_MESSAGE_SEVERITY_INFO
      };

      D3D12_INFO_QUEUE_FILTER filter = {};
      filter.DenyList.NumIDs = _countof(denyIds);
      filter.DenyList.pIDList = denyIds;
      filter.DenyList.NumSeverities = _countof(severities);
      filter.DenyList.pSeverityList = severities;

      infoQueue->PushStorageFilter(&filter);
    }
#endif

  }

  void DX12Basic::InitCommand()
  {
    HRESULT hr;

    // コマンドキューの設定
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // コマンドリストの種類
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // 優先度
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // フラグ
    commandQueueDesc.NodeMask = 0; // ノードマスク

    // コマンドキューの生成
    hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    pipelineCache_.Initialize(device_.Get());

    context_.Initialize(device_.Get(), &pipelineCache_, &uploadAllocator_);

    uploadAllocator_.Initialize(device_.Get(), kUploadAllocatorCapacity);
  }

  void DX12Basic::CreateSwapChain()
  {
    HRESULT hr;

    swapChainBufferCount_ = 2;

    // フリップモデルで VSync 無効の Present を行うには、対応確認のうえ生成時と Present 時の両方で許可フラグが要る
    BOOL allowTearing = FALSE;
    if (SUCCEEDED(dxgiFactory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
      isTearingSupported_ = allowTearing == TRUE;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::clientWidth; // 画面の幅
    swapChainDesc.Height = WinApp::clientHeight; // 画面の高さ
    swapChainDesc.Format = kColorFormat; // 色の形式(バックバッファのフォーマット)
    swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // バックバッファとして使用
    swapChainDesc.BufferCount = swapChainBufferCount_; // バッファ数
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後破棄
    swapChainDesc.Flags = SwapChainFlags();

    //コマンドキュー、ウィンドウハンドル、設定を渡して生成する
    hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(),
      winApp_->GetHWnd(),
      &swapChainDesc,
      nullptr,
      nullptr,
      reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    // ティアリング付き Present は排他フルスクリーンで使えないため、DXGI 標準の Alt+Enter 切り替えを止める
    dxgiFactory_->MakeWindowAssociation(winApp_->GetHWnd(), DXGI_MWA_NO_ALT_ENTER);
  }

  void DX12Basic::CreateDepthStencilResource()
  {
    depthStencilResource_ = MakeDepthBuffer(GetSceneWidth(), GetSceneHeight());
  }

  GpuResource DX12Basic::MakeDepthBuffer(uint32_t width, uint32_t height)
  {
    // テクスチャの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width; // テクスチャの幅
    resourceDesc.Height = height; // テクスチャの高さ
    resourceDesc.MipLevels = 1; // ミップマップレベル
    resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列サイズ
    resourceDesc.Format = kDepthFormat; // フォーマット
    resourceDesc.SampleDesc.Count = 1; // サンプル数、１固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 深度ステンシルとして使う

    // ヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM 上に作る

    // 深度値のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    // クリア値
    depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
    depthClearValue.Format = kDepthFormat; // フォーマット

    // Resource の生成
    GpuResource depthBuffer;
    HRESULT hr = device_->CreateCommittedResource(
      &heapProperties, // ヒープの設定
      D3D12_HEAP_FLAG_NONE, // Heap の特殊な設定。特になし
      &resourceDesc, // リソースの設定
      D3D12_RESOURCE_STATE_DEPTH_WRITE, // リソースの初期状態. DEPTH_WRITE
      &depthClearValue, // クリア値の設定.
      IID_PPV_ARGS(&depthBuffer.resource)); // 生成したリソースの pointer への pointer を取得
    assert(SUCCEEDED(hr));

    depthBuffer.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    return depthBuffer;
  }

  void DX12Basic::InitDescriptorHeap()
  {
    // RTV/DSV マネージャーの初期化（以降の InitSwapChainRTV / InitDSV が使用する）
    RtvManager::GetInstance()->Initialize(this);
    DsvManager::GetInstance()->Initialize(this);
  }

  void DX12Basic::InitSwapChainRTV()
  {
    for (UINT i = 0; i < kRtvHandleCount; ++i) {
      // SwapChain から Resource を取得
      HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i].resource));
      assert(SUCCEEDED(hr));
      swapChainResources_[i].state = D3D12_RESOURCE_STATE_PRESENT;

      // 初回のみ RTV 枠を確保し、リサイズ時は同じ枠へビューを作り直す
      if (swapChainRtvIndices_[i] == RtvManager::kInvalidIndex) {
        swapChainRtvIndices_[i] = RtvManager::GetInstance()->Allocate();
      }
      RtvManager::GetInstance()->CreateRTV(swapChainRtvIndices_[i], swapChainResources_[i].Get(), kColorFormat);
      rtvHandle_[i] = RtvManager::GetInstance()->GetCpuHandle(swapChainRtvIndices_[i]);
    }
  }

  void DX12Basic::InitDSV()
  {
    // 初回のみ DSV 枠を確保し、リサイズ時は同じ枠へビューを作り直す
    if (mainDsvIndex_ == DsvManager::kInvalidIndex) {
      mainDsvIndex_ = DsvManager::GetInstance()->Allocate();
    }
    DsvManager::GetInstance()->CreateDSV(mainDsvIndex_, depthStencilResource_.Get(), kDepthFormat);
  }

  void DX12Basic::InitFence()
  {
    // Fence の初期値
    fenceValue_ = 0;

    // フェンスの生成
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    // イベントの生成
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
  }

  void DX12Basic::InitViewport()
  {
    // ビューポートの設定
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.Width = static_cast<float>(WinApp::clientWidth);
    viewport_.Height = static_cast<float>(WinApp::clientHeight);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
  }

  void DX12Basic::InitScissorRect()
  {
    // シザー矩形の設定
    scissorRect_.left = 0;
    scissorRect_.right = WinApp::clientWidth;
    scissorRect_.top = 0;
    scissorRect_.bottom = WinApp::clientHeight;
  }

  void DX12Basic::InitFPSLimiter()
  {
    // 現在の時間を記録
    referenceTime_ = std::chrono::steady_clock::now();

  }

  void DX12Basic::UpdateFPSLimiter()
  {
    if (targetFPS_ <= 0) {
      referenceTime_ = std::chrono::steady_clock::now();
      return;
    }

    const std::chrono::microseconds kMinFrameTime(static_cast<uint64_t>(1000000.0 / targetFPS_));
    // 目標より少し速いフレームのみ待機させる閾値（+5fps 相当）
    const std::chrono::microseconds kMinCheckTime(static_cast<uint64_t>(1000000.0 / (targetFPS_ + 5)));

    // 現在の時間を取得
    auto currentTime = std::chrono::steady_clock::now();
    // 経過時間を取得
    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - referenceTime_);

    if (elapsedTime < kMinCheckTime) {
      // Sleep は OS のタイマー刻みまで寝過ごす（実測で最大約 2.6ms）ため、余裕がある間だけ寝て残りはスピンで合わせる
      const std::chrono::milliseconds kSleepMargin(2);
      const auto wakeTime = referenceTime_ + kMinFrameTime;
      while (std::chrono::steady_clock::now() + kSleepMargin < wakeTime) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      while (std::chrono::steady_clock::now() < wakeTime) {
        YieldProcessor();
      }
    }

    // 現在の時間を更新
    referenceTime_ = std::chrono::steady_clock::now();
  }

  void DX12Basic::RecreateDepthBuffer()
  {
    depthStencilResource_ = MakeDepthBuffer(GetSceneWidth(), GetSceneHeight());

    // 同じ DSV 枠へビューを作り直す
    InitDSV();

    RecreateNonEffectDepthBuffer();
  }

  void DX12Basic::RecreateNonEffectDepthBuffer()
  {
    nonEffectDepthResource_.Reset();

    // 等倍ならシーンの深度をそのまま共有する（非エフェクトパスの 3D もシーンと前後関係を取れる）
    if (GetSceneWidth() == static_cast<uint32_t>(WinApp::clientWidth) && GetSceneHeight() == static_cast<uint32_t>(WinApp::clientHeight)) {
      return;
    }

    nonEffectDepthResource_ = MakeDepthBuffer(static_cast<uint32_t>(WinApp::clientWidth), static_cast<uint32_t>(WinApp::clientHeight));
    if (nonEffectDsvIndex_ == DsvManager::kInvalidIndex) {
      nonEffectDsvIndex_ = DsvManager::GetInstance()->Allocate();
    }
    DsvManager::GetInstance()->CreateDSV(nonEffectDsvIndex_, nonEffectDepthResource_.Get(), kDepthFormat);
  }

  void DX12Basic::SetViewPort()
  {
    ID3D12GraphicsCommandList* commandList = context_.GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
  }

  void DX12Basic::BindSceneRenderTarget(const SceneRenderTarget& target)
  {
    sceneRenderTarget_ = target;
    RestoreSceneRenderTarget();
  }

  void DX12Basic::RestoreSceneRenderTarget()
  {
    const SceneRenderTarget& target = sceneRenderTarget_;
    context_.SetRenderTarget(&target.rtv, target.rtvFormat, target.dsv.ptr != 0 ? &target.dsv : nullptr, target.dsvFormat);
    ID3D12GraphicsCommandList* commandList = context_.GetCommandList();

    const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(target.width), static_cast<float>(target.height), 0.0f, 1.0f };
    const D3D12_RECT scissorRect{ 0, 0, static_cast<LONG>(target.width), static_cast<LONG>(target.height) };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
  }

  void DX12Basic::SetRenderScale(float scale)
  {
    scale = std::clamp(scale, kMinRenderScale, 1.0f);
    if (scale == renderScale_) {
      return;
    }
    renderScale_ = scale;
    RequestRenderTargetRebuild();
  }

  uint32_t DX12Basic::GetSceneWidth() const
  {
    return (std::max)(1u, static_cast<uint32_t>(std::lround(WinApp::clientWidth * renderScale_)));
  }

  uint32_t DX12Basic::GetSceneHeight() const
  {
    return (std::max)(1u, static_cast<uint32_t>(std::lround(WinApp::clientHeight * renderScale_)));
  }

  D3D12_CPU_DESCRIPTOR_HANDLE DX12Basic::GetMainDSVHandle() const
  {
    return DsvManager::GetInstance()->GetCpuHandle(mainDsvIndex_);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE DX12Basic::GetNonEffectDSVHandle() const
  {
    return nonEffectDepthResource_ ? DsvManager::GetInstance()->GetCpuHandle(nonEffectDsvIndex_) : GetMainDSVHandle();
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> DX12Basic::MakeBufferResource(size_t sizeInBytes)
  {
    // バッファの設定
    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeInBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // ヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    // リソースの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;
    //Microsoft::WRL::ComPtr<ID3D12Resource>

    HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&bufferResource));
    assert(SUCCEEDED(hr));

    return bufferResource;
  }

  void DX12Basic::CreateBufferResource(ComPtr<ID3D12Resource>& buffer, size_t sizeInBytes)
  {
    // バッファの設定
    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeInBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // ヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&buffer));
    assert(SUCCEEDED(hr));

  }

  void DX12Basic::CreateResourceForUAV(GpuResource& uavResource, UINT sizeInBytes)
  {
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeInBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device_->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_COMMON,
      nullptr,
      IID_PPV_ARGS(&uavResource.resource));
    assert(SUCCEEDED(hr));
    uavResource.state = D3D12_RESOURCE_STATE_COMMON;
  }

  void DX12Basic::CreateRenderTextureResource(GpuResource& rendertextureResource, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
  {
    // テクスチャの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width; // テクスチャの幅
    resourceDesc.Height = height; // テクスチャの高さ
    resourceDesc.DepthOrArraySize = 1; // 配列サイズ
    resourceDesc.MipLevels = 1; // ミップマップレベル
    resourceDesc.Format = format; // フォーマット
    resourceDesc.SampleDesc.Count = 1; // サンプル数
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2D テクスチャ
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // レンダーターゲットとして使う

    // ヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM 上に作る

    // クリア値の設定
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    // Resource の生成
    HRESULT hr = device_->CreateCommittedResource(
      &heapProperties, // ヒープの設定
      D3D12_HEAP_FLAG_NONE, // Heap の特殊な設定。特になし
      &resourceDesc, // リソースの設定
      D3D12_RESOURCE_STATE_RENDER_TARGET, // リソースの初期状態. レンダーターゲットとして使う
      &clearValue, // クリア値の設定
      IID_PPV_ARGS(&rendertextureResource.resource)); // 生成したリソースの pointer への pointer を取得
    assert(SUCCEEDED(hr));
    rendertextureResource.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE DX12Basic::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE DX12Basic::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSize * index);
    return handle;
  }

  void DX12Basic::ResizeBuffers(uint32_t width, uint32_t height)
  {
    // GPU の処理が完了するまで待機
    WaitForGPU();

    // RTV に関連するリソースをクリア
    for (UINT i = 0; i < swapChainBufferCount_; i++) {
      swapChainResources_[i].Reset();
    }

    // 深度バッファをクリア
    depthStencilResource_.Reset();

    // スワップチェインのリサイズ
    HRESULT hr = swapChain_->ResizeBuffers(
      swapChainBufferCount_,
      width,
      height,
      DXGI_FORMAT_UNKNOWN,  // 既存のフォーマットを使用
      SwapChainFlags()      // 生成時と揃えないとティアリング許可が外れる
    );
    assert(SUCCEEDED(hr));

    // RTV を再作成
    InitSwapChainRTV();

    // 深度バッファを再作成
    RecreateDepthBuffer();

    // ビューポートとシザー矩形を更新
    UpdateViewportAndScissorRect();
  }

} // namespace Tako

