#include "DX12Basic.h"
#include <cassert>
#include "Logger.h"
#include "StringUtility.h"
#include <format>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

void DX12Basic::Initialize(WinApp* winApp)
{
	assert(winApp);
	winApp_ = winApp;
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

		infoQueue->Release();
	}
#endif

}

void DX12Basic::CommandInit()
{
	HRESULT hr;

	// CommandQueueの生成
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
	assert(SUCCEEDED(hr));

	// CommandAllocatorの生成
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	assert(SUCCEEDED(hr));

	// CommandListの生成
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	assert(SUCCEEDED(hr));
}

void DX12Basic::CreateSwapChain()
{
	HRESULT hr;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = WinApp::kClientWidth; // 画面の幅
	swapChainDesc.Height = WinApp::kClientHeight; // 画面の高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式(バックバッファのフォーマット)
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // バックバッファとして使用
	swapChainDesc.BufferCount = 2; // バッファ数
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

