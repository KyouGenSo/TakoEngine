#include "DX12Basic.h"
#include <cassert>
#include "Logger.h"
#include "StringUtility.h"
#include <format>

#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_win32.h"
#include"externals/imgui/imgui_impl_dx12.h"

#include"externals/DirectXTex/d3dx12.h"
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

void DX12Basic::Initialize(WinApp* winApp)
{
	assert(winApp);
	this->winApp_ = winApp;

	commandQueue_ = nullptr;
	commandList_ = nullptr;
	commandAllocator_ = nullptr;

	// Deviceの初期化
	DeviceInit();

	// Command関連の初期化
	CommandInit();

	// スワップチェインの生成
	CreateSwapChain();

	// 深度バッファの生成
	CreateDepthStencilResource();

	// デスクリプタヒープの初期化
	DescriptorHeapInit();

	// レンダーターゲットビューの初期化
	RTVInit();

	// 深度ステンシルビューの初期化
	DSVInit();

	// Fenceの初期化
	FenceInit();

	// ビューポート矩形の初期化
	ViewportInit();

	// シザー矩形の初期化
	ScissorRectInit();

	// DXCコンパイラの生成
	CreateDXCCompiler();

	// ImGuiの初期化
	ImGuiInit();

}

void DX12Basic::BeginDraw()
{
	// バッグバッファのインデックスを取得
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	// ---PRESENTからRTV用のStateにするTransitionBarrierを張る--- //
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	// 遷移前（現在）のresourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	// 遷移後のresourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// バリアを張る
	commandList_->ResourceBarrier(1, &barrier);
	// ---PRESENTからRTV用のStateにするTransitionBarrierを張る--- //

	// 描画先のRTVとDSVを設定
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList_->OMSetRenderTargets(1, &rtvHandle_[backBufferIndex], false, &dsvHandle);

	// クリアカラー
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

	// 画面の色をクリア
	commandList_->ClearRenderTargetView(rtvHandle_[backBufferIndex], clearColor, 0, nullptr);

	// 深度ステンシルをクリア
	commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// SRVのヒープを指定
	ID3D12DescriptorHeap* heaps[] = { srvHeap_.Get() };
	commandList_->SetDescriptorHeaps(_countof(heaps), heaps);

	// ビューポートとシザリング矩形をセット
	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);


}

void DX12Basic::EndDraw()
{
	HRESULT	hr;

	// バッグバッファのインデックスを取得
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	// ---RTVからPRESENT用のStateにするTransitionBarrierを張る--- //
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	// 遷移前（現在
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// 遷移後
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	// バリアを張る
	commandList_->ResourceBarrier(1, &barrier);
	// ---RTVからPRESENT用のStateにするTransitionBarrierを張る--- //

	// コマンドリストのクローズ
	hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);

	// GPUとOSに画面の交換を行うよう通知する
	swapChain_->Present(1, 0);

	//Fenceの値を更新
	fenceValue_++;
	// コマンドキューにFenceのシグナルを行う
	commandQueue_->Signal(fence_.Get(), fenceValue_);

	// コマンド完了まで待機
	if (fence_->GetCompletedValue() < fenceValue_)
	{
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	// コマンドアロケータをリセット
	hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));

	// コマンドリストをリセット
	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));

}

void DX12Basic::DeviceInit()
{
	HRESULT hr;

	//-----------------------------------------------------デバッグレイヤーの設定-----------------------------------------------------
#ifdef _DEBUG
	ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	//------------------------------------------------------DXGIファクトリの生成------------------------------------------------------
	hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	//-----------------------------------------------------------アダプターの列挙-----------------------------------------------------------
	ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	for (UINT adapterIndex = 0; dxgiFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		//アダプターの情報を取得
		DXGI_ADAPTER_DESC3 desc;
		hr = useAdapter->GetDesc3(&desc);
		assert(SUCCEEDED(hr));

		if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			Logger::Log(std::format("Use Adapter:{}\n", StringUtility::ConvertString(desc.Description)));
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
			Logger::Log(std::format("Feature Level: {}\n", featureLevelNames[i]));
			break;
		}
	}
	assert(device_ != nullptr);
	Logger::Log("D3D12Device Created\n");


	//-----------------------------------------------------エラーチェック-----------------------------------------------------
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		// デバッグレイヤーの設定
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
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

		//infoQueue->Release();
	}
#endif

}

void DX12Basic::CommandInit()
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

	// コマンドアロケータの生成
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	assert(SUCCEEDED(hr));

	// コマンドリストの生成
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	assert(SUCCEEDED(hr));


}

void DX12Basic::CreateSwapChain()
{
	HRESULT hr;

	swapChainBufferCount_ = 2;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = WinApp::kClientWidth; // 画面の幅
	swapChainDesc.Height = WinApp::kClientHeight; // 画面の高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式(バックバッファのフォーマット)
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // バックバッファとして使用
	swapChainDesc.BufferCount = swapChainBufferCount_; // バッファ数
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後破棄

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue_.Get(),
		winApp_->GetHWnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())
	);
	assert(SUCCEEDED(hr));
}

void DX12Basic::CreateDepthStencilResource()
{
	// テクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = WinApp::kClientWidth; // テクスチャの幅
	resourceDesc.Height = WinApp::kClientHeight; // テクスチャの高さ
	resourceDesc.MipLevels = 1; // ミップマップレベル
	resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列サイズ
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプル数、１固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 深度ステンシルとして使う

	// ヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	// クリア値
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット

	// Resourceの生成
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties, // ヒープの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // リソースの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // リソースの初期状態. DEPTH_WRITE
		&depthClearValue, // クリア値の設定.
		IID_PPV_ARGS(&depthStencilResource_)); // 生成したリソースのpointerへのpointerを取得
	assert(SUCCEEDED(hr));

}

void DX12Basic::DescriptorHeapInit()
{
	// ディスクリプタヒープのサイズを取得
	descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// RTVのディスクリプタヒープの生成
	rtvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// DSVのディスクリプタヒープの生成
	dsvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// SRVのディスクリプタヒープの生成
	srvHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
}

void DX12Basic::RTVInit()
{
	// SwapChainからResourceを取得
	for (UINT i = 0; i < 2; ++i)
	{
		HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		assert(SUCCEEDED(hr));
	}

	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	// DescriptorHeapの先頭を取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = GetCPUDescriptorHandle(rtvHeap_.Get(), descriptorSizeRTV_, 0);

	for (UINT i = 0; i < kRtvHandleCount_; ++i)
	{
		// RTVのハンドルを取得
		if (i == 0) {
			rtvHandle_[i] = rtvStartHandle;
		} else {
			rtvHandle_[i].ptr += rtvHandle_[i - 1].ptr + descriptorSizeRTV_;
		}

		// RTVの作成
		device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc, rtvHandle_[i]);
	}

}

void DX12Basic::DSVInit()
{
	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	// DSVのハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

	// DSVの作成
	device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle);
}

void DX12Basic::FenceInit()
{
	// Fenceの初期値
	fenceValue_ = 0;

	// フェンスの生成
	HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));

	// イベントの生成
	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);
}

void DX12Basic::ViewportInit()
{
	// ビューポートの設定
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.Width = static_cast<float>(WinApp::kClientWidth);
	viewport_.Height = static_cast<float>(WinApp::kClientHeight);
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

void DX12Basic::ScissorRectInit()
{
	// シザー矩形の設定
	scissorRect_.left = 0;
	scissorRect_.right = WinApp::kClientWidth;
	scissorRect_.top = 0;
	scissorRect_.bottom = WinApp::kClientHeight;
}

void DX12Basic::CreateDXCCompiler()
{
	HRESULT hr;
	// DXCUtillitiesの生成
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));

	// DXCコンパイラの生成
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	// IncludeHandlerの生成
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));

}

void DX12Basic::ImGuiInit()
{
	// バージョンチェック
	IMGUI_CHECKVERSION();

	// コンテキストの生成
	ImGui::CreateContext();

	// スタイルの設定
	ImGui::StyleColorsDark();

	// Win32用の初期化
	ImGui_ImplWin32_Init(winApp_->GetHWnd());

	// DX12用の初期化
	ImGui_ImplDX12_Init(
		device_.Get(),                                   // デバイス
		swapChainBufferCount_,                           // バッファ数
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,                 // RTVのフォーマット
		srvHeap_.Get(),                                  // SRVのヒープ
		srvHeap_->GetCPUDescriptorHandleForHeapStart(),  // SRVのCPUハンドルの開始位置
		srvHeap_->GetGPUDescriptorHandleForHeapStart()); // SRVのGPUハンドルの開始位置

}

ID3D12DescriptorHeap* DX12Basic::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	// ヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = heapType;
	heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// ヒープの生成
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Basic::GetSRVCpuDescriptorHandle(uint32_t index)
{
	return GetCPUDescriptorHandle(srvHeap_.Get(), descriptorSizeSRV_, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12Basic::GetSRVGpuDescriptorHandle(uint32_t index)
{
	return GetGPUDescriptorHandle(srvHeap_.Get(), descriptorSizeSRV_, index);
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

