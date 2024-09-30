#include<Windows.h>
#include<wrl.h>
#include<cstdint>
#include<string>
#include<fstream>
#include<sstream>
#include<format>
#include <unordered_map>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include<dxcapi.h>
#include <vector>
#include <dxgidebug.h>
#include"externals/DirectXTex/d3dx12.h"
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include "xaudio2.h"
#pragma comment(lib, "xaudio2.lib")

#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_win32.h"
#include"externals/imgui/imgui_impl_dx12.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include"Vector4.h"
#include"Vector2.h"
#include"Matrix4x4.h"
#include"Matrix4x4Function.h"
#include"Vector3Function.h"

#include"externals/DirectXTex/DirectXTex.h"

#define PI 3.14159265359f

// ComPtrのエイリアス
template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


//クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct VertexDataNoTex
{
	Vector4 position;
	Vector3 normal;
};

struct VertexHash {
	size_t operator()(const VertexData& vertex) const {
		size_t h1 = std::hash<float>{}(vertex.position.x);
		size_t h2 = std::hash<float>{}(vertex.position.y);
		size_t h3 = std::hash<float>{}(vertex.position.z);
		size_t h4 = std::hash<float>{}(vertex.position.w);
		size_t h5 = std::hash<float>{}(vertex.texcoord.x);
		size_t h6 = std::hash<float>{}(vertex.texcoord.y);
		return h1 ^ h2 ^ h3 ^ h4 ^ h5 ^ h6;
	}
};

// 自定义相等比较函数
struct VertexEqual {
	bool operator()(const VertexData& lhs, const VertexData& rhs) const {
		return lhs.position.x == rhs.position.x &&
			lhs.position.y == rhs.position.y &&
			lhs.position.z == rhs.position.z &&
			lhs.position.w == rhs.position.w &&
			lhs.texcoord.x == rhs.texcoord.x &&
			lhs.texcoord.y == rhs.texcoord.y;
	}
};

struct Material
{
	Vector4 color;
	bool enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 world;
};

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	int32_t lightType;
	float intensity;
};

struct MaterialData {
	std::string texturePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct ModelDataNoTex {
	std::vector<VertexDataNoTex> vertices;
};

struct D3DResourceLeakCheker
{
	~D3DResourceLeakCheker()
	{
		ComPtr<IDXGIDebug1> dxgiDebug = nullptr;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}

	int mLeakCount = 0;
};

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
void Log(const std::string& messege);

std::wstring ConvertString(const std::string& str);

std::string ConvertString(const std::wstring& str);

//CompileShader
IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler);

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

DirectX::ScratchImage LoadTexture(const std::string& filePath);

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metaData);

[[nodiscard]]
ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

ID3D12Resource* CreateDepthStencilResource(ID3D12Device* device, int32_t width, int32_t height);

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

ModelData LoadObjFile(const std::string& directoryPath, const std::string& fileName);

MaterialData LoadMtlFile(const std::string& directoryPath, const std::string& fileName);

std::vector<ModelData> LoadMutiMeshObjFile(const std::string& directoryPath, const std::string& fileName);

std::vector<ModelData> LoadMutiMaterialFile(const std::string& directoryPath, const std::string& fileName);

std::unordered_map<std::string, MaterialData> LoadMutiMaterialMtlFile(const std::string& directoryPath, const std::string& fileName);

ModelDataNoTex LoadObjFileNoTex(const std::string& directoryPath, const std::string& fileName);

SoundData LoadWaveFile(const char* filename);

void SoundUnload(SoundData* soundData);

void SoundPlay(IXAudio2* xAudio2, const SoundData& soundData);

//-----------------------------------------FUNCTION-----------------------------------------//


//------------------------------------------WINDOW------------------------------------------//
//ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wparam, lparam))
	{
		return true;
	}
	//メッセージによって処理を分岐
	switch (msg)
	{
		//ウィンドウが破棄されたとき
	case WM_DESTROY:
		//メッセージループを終了
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, msg, wparam, lparam);
}



//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	D3DResourceLeakCheker leakChecker;
	// COMの初期化
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	WNDCLASS wc{};
	//ウィンドウプロシージャ
	wc.lpfnWndProc = WndProc;
	//クラス名
	wc.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	//ウィンドウクラスを登録
	RegisterClass(&wc);

	//ウィンドウサイズを表す構造体にクライアント領域のサイズを入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//ウィンドウの生成
	HWND hWnd = CreateWindow(
		wc.lpszClassName,	    //クラス名
		L"CG2",	                //タイトルバーの文字列
		WS_OVERLAPPEDWINDOW,	//ウィンドウスタイル
		CW_USEDEFAULT,		    //表示X座標
		CW_USEDEFAULT,		    //表示Y座標
		wrc.right - wrc.left,	//ウィンドウ幅
		wrc.bottom - wrc.top,	//ウィンドウ高さ
		nullptr,		        //親ウィンドウハンドル
		nullptr,		        //メニューハンドル
		wc.hInstance,		    //インスタンスハンドル
		nullptr);		        //追加パラメータ

	//ウィンドウを表示
	ShowWindow(hWnd, SW_SHOW);
	//-----------------------------------------WINDOW-----------------------------------------//

	//-----------------------------------------DebugLayer-----------------------------------------//
#ifdef _DEBUG
	ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif
	//-----------------------------------------DebugLayer-----------------------------------------//


	//-----------------------------------------DirectX-----------------------------------------//
	//DXGIファクトリの生成
	ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	//アダプターの列挙
	ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	for (UINT adapterIndex = 0; dxgiFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		//アダプターの情報を取得
		DXGI_ADAPTER_DESC3 desc;
		hr = useAdapter->GetDesc3(&desc);
		assert(SUCCEEDED(hr));

		if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			Log(std::format("Use Adapter:{}\n", ConvertString(desc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	// デバイスの生成
	ComPtr<ID3D12Device> device = nullptr;
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
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			Log(std::format("Feature Level: {}\n", featureLevelNames[i]));
			break;
		}
	}
	assert(device != nullptr);
	Log("D3D12Device Created\n");

#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
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

	// CommandQueueの生成
	ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	// CommandAllocatorの生成
	ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	// CommandListの生成
	ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	// SwapChainの生成
	ComPtr<IDXGISwapChain4> swapChain = nullptr;
	//IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;   // 画面の幅
	swapChainDesc.Height = kClientHeight; // 画面の高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式(バックバッファのフォーマット)
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // バックバッファとして使用
	swapChainDesc.BufferCount = 2; // バッファ数
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後破棄
	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// DescriptorHeapの生成
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// DescriptorSizeの取得
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// SwapChainからResourceを取得
	ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	//ID3D12Resource* swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// RTVの生成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む
	// DescriptorHeapの先頭を取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = GetCPUDescriptorHandle(rtvDescriptorHeap.Get(), descriptorSizeRTV, 0);
	// RTVを二つ作るのでDescriptorSizeを二つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle[2];
	rtvHandle[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandle[0]);
	rtvHandle[1].ptr = rtvHandle[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandle[1]);

	//FenceとEventの生成
	ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent != nullptr);


	//-----------------------------------------DXC初期化-----------------------------------------//
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
	//-----------------------------------------DXC初期化-----------------------------------------//

	//-----------------------------------------/PSO-----------------------------------------///
	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大LOD
	samplerDesc[0].ShaderRegister = 0; // レジスタ番号
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	descriptionRootSignature.pStaticSamplers = samplerDesc;
	descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

	// DescriptorRangeの設定。
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド 

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // ディスクリプタレンジを設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // レンジの数

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ComPtr<ID3DBlob> signatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
	signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf());
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// shaderのコンパイル
	IDxcBlob* vertexShaderBlob = CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// depthの機能を有効化にする
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 深度の比較方法
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	// textureがない場合のPSOを生成---------------------------------------------------------
	
	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignatureNoTex{};
	descriptionRootSignatureNoTex.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParametersNoTex[3] = {};

	// WVP行列.VertexShaderで使う
	rootParametersNoTex[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParametersNoTex[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParametersNoTex[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	// materialの設定.PixelShaderで使う
	rootParametersNoTex[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParametersNoTex[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParametersNoTex[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	// DirectionalLightの設定.PixelShaderで使う
	rootParametersNoTex[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParametersNoTex[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParametersNoTex[2].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド

	descriptionRootSignatureNoTex.pParameters = rootParametersNoTex;
	descriptionRootSignatureNoTex.NumParameters = _countof(rootParametersNoTex);

	ComPtr<ID3DBlob> signatureBlobNoTex = nullptr;
	ComPtr<ID3DBlob> errorBlobNoTex = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignatureNoTex, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlobNoTex, &errorBlobNoTex);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlobNoTex->GetBufferPointer()));
		assert(false);
	}

	ComPtr<ID3D12RootSignature> rootSignatureNoTex = nullptr;
	hr = device->CreateRootSignature(0, signatureBlobNoTex->GetBufferPointer(), signatureBlobNoTex->GetBufferSize(), IID_PPV_ARGS(rootSignatureNoTex.GetAddressOf()));
	signatureBlobNoTex->GetBufferSize(), IID_PPV_ARGS(rootSignatureNoTex.GetAddressOf());
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescsNoTex[2] = {};
	inputElementDescsNoTex[0].SemanticName = "POSITION";
	inputElementDescsNoTex[0].SemanticIndex = 0;
	inputElementDescsNoTex[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescsNoTex[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescsNoTex[1].SemanticName = "NORMAL";
	inputElementDescsNoTex[1].SemanticIndex = 0;
	inputElementDescsNoTex[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescsNoTex[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDescNoTex{};
	inputLayoutDescNoTex.pInputElementDescs = inputElementDescsNoTex;
	inputLayoutDescNoTex.NumElements = _countof(inputElementDescsNoTex);

	// BlendState
	D3D12_BLEND_DESC blendDescNoTex{};
	blendDescNoTex.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDescNoTex{};
	// 三角形の中を塗りつぶす
	rasterizerDescNoTex.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面を表示しない
	rasterizerDescNoTex.CullMode = D3D12_CULL_MODE_BACK;

	// shaderのコンパイル
	IDxcBlob* vertexShaderNoTexBlob = CompileShader(L"resources/shaders/NoTex.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderNoTexBlob != nullptr);

	IDxcBlob* pixelShaderNoTexBlob = CompileShader(L"resources/shaders/NoTex.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderNoTexBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDescNoTex{};
	// depthの機能を有効化にする
	depthStencilDescNoTex.DepthEnable = true;
	// 書き込みします
	depthStencilDescNoTex.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 深度の比較方法
	depthStencilDescNoTex.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateNoTexDesc{};
	graphicsPipelineStateNoTexDesc.pRootSignature = rootSignatureNoTex.Get();
	graphicsPipelineStateNoTexDesc.InputLayout = inputLayoutDescNoTex;
	graphicsPipelineStateNoTexDesc.VS = { vertexShaderNoTexBlob->GetBufferPointer(), vertexShaderNoTexBlob->GetBufferSize() };
	graphicsPipelineStateNoTexDesc.PS = { pixelShaderNoTexBlob->GetBufferPointer(), pixelShaderNoTexBlob->GetBufferSize() };
	graphicsPipelineStateNoTexDesc.BlendState = blendDescNoTex;
	graphicsPipelineStateNoTexDesc.RasterizerState = rasterizerDescNoTex;
	// 書き込むRTVの情報
	graphicsPipelineStateNoTexDesc.NumRenderTargets = 1;
	graphicsPipelineStateNoTexDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateNoTexDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateNoTexDesc.SampleDesc.Count = 1;
	graphicsPipelineStateNoTexDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsPipelineStateNoTexDesc.DepthStencilState = depthStencilDescNoTex;
	graphicsPipelineStateNoTexDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	ComPtr<ID3D12PipelineState> graphicsPipelineStateNoTex = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateNoTexDesc, IID_PPV_ARGS(&graphicsPipelineStateNoTex));
	assert(SUCCEEDED(hr));

	//-----------------------------------------PSO-----------------------------------------///


	//-------------------------------------------------------------------------------------------------------------------
	//                                                                                                                   
	//                                                  Resourceの作成                                                    
	//                                                                                                                   
	//-------------------------------------------------------------------------------------------------------------------

	//------------------------------------------------------Model------------------------------------------------------
	enum ModelType {
		Plane,
		Teapot,
		Bunny,
		MultiMesh,
		MultiMaterial,
		Suzanne,
	};

	ModelType modelType = Plane;

	// Planeのデータを読み込む------------------------------------------------------------
	ModelData planeData = LoadObjFile("resources", "plane.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> planeVertexResource = CreateBufferResource(device.Get(), sizeof(VertexData) * planeData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW planeVertexBufferView{};
	planeVertexBufferView.BufferLocation = planeVertexResource->GetGPUVirtualAddress();
	planeVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * planeData.vertices.size());
	planeVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* planeVertexData = nullptr;
	planeVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&planeVertexData));
	memcpy(planeVertexData, planeData.vertices.data(), sizeof(VertexData) * planeData.vertices.size());


	// Teapotのデータを読み込む------------------------------------------------------------
	ModelData teapotData = LoadObjFile("resources", "teapot.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> teapotVertexResource = CreateBufferResource(device.Get(), sizeof(VertexData) * teapotData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW teapotVertexBufferView{};
	teapotVertexBufferView.BufferLocation = teapotVertexResource->GetGPUVirtualAddress();
	teapotVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * teapotData.vertices.size());
	teapotVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* teapotVertexData = nullptr;
	teapotVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&teapotVertexData));
	memcpy(teapotVertexData, teapotData.vertices.data(), sizeof(VertexData) * teapotData.vertices.size());


	// Bunnyのデータを読み込む---------------------------------------------------------
	ModelData bunnyData = LoadObjFile("resources", "bunny.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> bunnyVertexResource = CreateBufferResource(device.Get(), sizeof(VertexData) * bunnyData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW bunnyVertexBufferView{};
	bunnyVertexBufferView.BufferLocation = bunnyVertexResource->GetGPUVirtualAddress();
	bunnyVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * bunnyData.vertices.size());
	bunnyVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* bunnyVertexData = nullptr;
	bunnyVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&bunnyVertexData));
	memcpy(bunnyVertexData, bunnyData.vertices.data(), sizeof(VertexData) * bunnyData.vertices.size());


	// Mutimesh.objデータを読み込む---------------------------------------------------------
	std::vector<ModelData> multiMeshModelDatas = LoadMutiMeshObjFile("resources", "multiMesh.obj");

	std::vector<ComPtr<ID3D12Resource>> multiMeshVertexResources;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> multiMeshVertexBufferViews;

	for (const auto& modelData : multiMeshModelDatas) {
		ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(device.Get(), sizeof(VertexData) * modelData.vertices.size());
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* vertexData = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

		multiMeshVertexResources.push_back(vertexResource);
		multiMeshVertexBufferViews.push_back(vertexBufferView);
	}

	// MutiMaterial.objデータを読み込む---------------------------------------------------------
	std::vector<ModelData> multiMaterialModelDatas = LoadMutiMaterialFile("resources", "multiMaterial.obj");

	std::vector<ComPtr<ID3D12Resource>> multiMaterialVertexResources;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> multiMaterialVertexBufferViews;

	for (const auto& modelData : multiMaterialModelDatas) {
		ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(device.Get(), sizeof(VertexData) * modelData.vertices.size());
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* vertexData = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

		multiMaterialVertexResources.push_back(vertexResource);
		multiMaterialVertexBufferViews.push_back(vertexBufferView);
	}

	// Suzanneのデータを読み込む---------------------------------------------------------
	ModelDataNoTex suzanneData = LoadObjFileNoTex("resources", "suzanne.obj");

	// 頂点リソースを作る
	ComPtr<ID3D12Resource> suzanneVertexResource = CreateBufferResource(device.Get(), sizeof(VertexDataNoTex) * suzanneData.vertices.size());

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW suzanneVertexBufferView{};
	suzanneVertexBufferView.BufferLocation = suzanneVertexResource->GetGPUVirtualAddress();
	suzanneVertexBufferView.SizeInBytes = UINT(sizeof(VertexDataNoTex) * suzanneData.vertices.size());
	suzanneVertexBufferView.StrideInBytes = sizeof(VertexDataNoTex);

	// 頂点リソースにデータを書き込む
	VertexDataNoTex* suzanneVertexData = nullptr;
	suzanneVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&suzanneVertexData));
	memcpy(suzanneVertexData, suzanneData.vertices.data(), sizeof(VertexDataNoTex) * suzanneData.vertices.size());

	// 球のリソースを作る----------------------------------------------------------------------------------------------
	const int kVertexCount = 16 * 16 * 6;

	ComPtr<ID3D12Resource> sphereVertexResource = CreateBufferResource(device.Get(), sizeof(VertexData) * kVertexCount);

	//VertexBufferView
	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	// リソースの先頭アドレスから使う
	sphereVertexBufferView.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点三つ分のサイズ
	sphereVertexBufferView.SizeInBytes = sizeof(VertexData) * kVertexCount;
	// 一つの頂点のサイズ
	sphereVertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* sphereVertexData = nullptr;
	// アドレスを取得
	sphereVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereVertexData));

	// 球を作成(vertexIndex Version)-------------------------------------------------------------//
	ComPtr<ID3D12Resource> sphereIndexResource = CreateBufferResource(device.Get(), sizeof(uint32_t) * kVertexCount);
	D3D12_INDEX_BUFFER_VIEW sphereIndexBufferView{};
	sphereIndexBufferView.BufferLocation = sphereIndexResource->GetGPUVirtualAddress();
	sphereIndexBufferView.SizeInBytes = sizeof(uint32_t) * kVertexCount;
	sphereIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexData = nullptr;
	sphereIndexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));


	const uint32_t kSubdivision = 16;
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	std::unordered_map<VertexData, uint32_t, VertexHash, VertexEqual> uniqueVertices;

	uint32_t uniqueVertexCount = 0;

	for (int latIndex = 0; latIndex < kSubdivision; latIndex++) {

		float theta = -DirectX::XM_PIDIV2 + kLatEvery * latIndex;

		for (int lonIndex = 0; lonIndex < kSubdivision; lonIndex++) {

			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
			float phi = kLonEvery * lonIndex;

			VertexData vertices[6];
			vertices[0] = { cos(theta) * cos(phi), sin(theta), cos(theta) * sin(phi), 1.0f, float(lonIndex) / kSubdivision, 1.0f - float(latIndex) / kSubdivision };
			vertices[1] = { cos(theta + kLatEvery) * cos(phi), sin(theta + kLatEvery), cos(theta + kLatEvery) * sin(phi), 1.0f, float(lonIndex) / kSubdivision, 1.0f - float(latIndex + 1) / kSubdivision };
			vertices[2] = { cos(theta) * cos(phi + kLonEvery), sin(theta), cos(theta) * sin(phi + kLonEvery), 1.0f, float(lonIndex + 1) / kSubdivision, 1.0f - float(latIndex) / kSubdivision };
			vertices[3] = { cos(theta) * cos(phi + kLonEvery), sin(theta), cos(theta) * sin(phi + kLonEvery), 1.0f, float(lonIndex + 1) / kSubdivision, 1.0f - float(latIndex) / kSubdivision };
			vertices[4] = { cos(theta + kLatEvery) * cos(phi), sin(theta + kLatEvery), cos(theta + kLatEvery) * sin(phi), 1.0f, float(lonIndex) / kSubdivision, 1.0f - float(latIndex + 1) / kSubdivision };
			vertices[5] = { cos(theta + kLatEvery) * cos(phi + kLonEvery), sin(theta + kLatEvery), cos(theta + kLatEvery) * sin(phi + kLonEvery), 1.0f, float(lonIndex + 1) / kSubdivision, 1.0f - float(latIndex + 1) / kSubdivision };

			for (int i = 0; i < 6; i++) {
				auto iter = uniqueVertices.find(vertices[i]);
				if (iter != uniqueVertices.end()) {
					indexData[start + i] = iter->second;
				} else {
					uniqueVertices[vertices[i]] = uniqueVertexCount;
					sphereVertexData[uniqueVertexCount] = vertices[i];
					indexData[start + i] = uniqueVertexCount;
					uniqueVertexCount++;
				}
			}
		}
	}

	for (int i = 0; i < kVertexCount; i++)
	{
		sphereVertexData[i].normal = Vector3(sphereVertexData[i].position.x, sphereVertexData[i].position.y, sphereVertexData[i].position.z);
	}

	sphereVertexBufferView.SizeInBytes = sizeof(VertexData) * uniqueVertexCount;
	sphereIndexBufferView.SizeInBytes = sizeof(uint32_t) * kVertexCount;
	//------------------------------------------------------Model------------------------------------------------------//


	//------------------------------------------------------Material------------------------------------------------------
	// マテリアル用のリソースを作る。--------------------------------------//
	ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device.Get(), sizeof(Material));
	//ID3D12Resource* materialResource = CreateBufferResource(device.Get(), sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// アドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 赤
	materialData[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// ライティングを有効にする
	materialData[0].enableLighting = true;
	// uvTransformを初期化
	materialData[0].uvTransform = MakeIdentityMatrix4x4();

	Transform uvTransformModel{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};
	//------------------------------------------------------Material------------------------------------------------------//

	// WVP用のCBufferリソースを作る。----------------------------------------------//
	ComPtr<ID3D12Resource> modelWvpResource = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));
	// WVPにデータを書き込む
	TransformationMatrix* modelTrasformationMatrixData = nullptr;
	// アドレスを取得
	modelWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&modelTrasformationMatrixData));
	// 単位行列を書き込んでいく
	modelTrasformationMatrixData->WVP = MakeIdentityMatrix4x4();
	modelTrasformationMatrixData->world = MakeIdentityMatrix4x4();

	Transform modelTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	ComPtr<ID3D12Resource> sphereWvpResource = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));

	// WVPにデータを書き込む
	TransformationMatrix* sphereTrasformationMatrixData = nullptr;
	// アドレスを取得
	sphereWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&sphereTrasformationMatrixData));

	// 単位行列を書き込んでいく
	sphereTrasformationMatrixData->WVP = MakeIdentityMatrix4x4();
	sphereTrasformationMatrixData->world = MakeIdentityMatrix4x4();

	Transform sphereTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// ---------------------------------------------------Texture---------------------------------------------------
	// uvCheckerの読み込み
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metaData = mipImages.GetMetadata();
	// Texture用のリソースを作成
	ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device.Get(), metaData);
	// Textureのデータを転送
	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureResource.Get(), mipImages, device.Get(), commandList.Get());

	// uvChecker用のSRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metaData.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = UINT(metaData.mipLevels);

	// uvCheckerのSRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);

	// uvCheckerのSRVを作成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	//-------------------------------------------------------------------------------------------------------//

	// planeのTextureの読み込み
	DirectX::ScratchImage planeMipImages = LoadTexture(planeData.material.texturePath);
	const DirectX::TexMetadata& metaData2 = planeMipImages.GetMetadata();
	// Texture用のリソースを作成
	ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device.Get(), metaData2);
	// Textureのデータを転送
	ComPtr<ID3D12Resource> intermediateResource2 = UploadTextureData(textureResource2.Get(), planeMipImages, device.Get(), commandList.Get());

	// SRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metaData2.format;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.Texture2D.MipLevels = UINT(metaData2.mipLevels);

	// SRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);

	// SRVを作成
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	//-------------------------------------------------------------------------------------------------------//

	// teapotのTextureの読み込み
	DirectX::ScratchImage teapotMipImages = LoadTexture(teapotData.material.texturePath);
	const DirectX::TexMetadata& metaData3 = teapotMipImages.GetMetadata();
	// Texture用のリソースを作成
	ComPtr<ID3D12Resource> textureResource3 = CreateTextureResource(device.Get(), metaData3);
	// Textureのデータを転送
	ComPtr<ID3D12Resource> intermediateResource3 = UploadTextureData(textureResource3.Get(), teapotMipImages, device.Get(), commandList.Get());

	// SRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metaData3.format;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.Texture2D.MipLevels = UINT(metaData3.mipLevels);

	// SRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);

	// SRVを作成
	device->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);

	//-------------------------------------------------------------------------------------------------------//
	// mutiMaterialのTextureの読み込み
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> textureSrvHandleCPUs;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textureSrvHandleGPUs;

	for (int i = 0; i < multiMaterialModelDatas.size(); i++) {
		DirectX::ScratchImage multiMateMipImages = LoadTexture(multiMaterialModelDatas[i].material.texturePath);
		const DirectX::TexMetadata& multiMateMetaData = multiMateMipImages.GetMetadata();
		// Texture用のリソースを作成
		ComPtr<ID3D12Resource> multiMateTextureResource = CreateTextureResource(device.Get(), multiMateMetaData);
		// Textureのデータを転送
		ComPtr<ID3D12Resource> multiMateIntermediateResource = UploadTextureData(multiMateTextureResource.Get(), multiMateMipImages, device.Get(), commandList.Get());

		// SRVを作成
		D3D12_SHADER_RESOURCE_VIEW_DESC multiMateSrvDesc{};
		multiMateSrvDesc.Format = multiMateMetaData.format;
		multiMateSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
		multiMateSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		multiMateSrvDesc.Texture2D.MipLevels = UINT(multiMateMetaData.mipLevels);

		// SRVを作成するDescriptorの位置を決める
		D3D12_CPU_DESCRIPTOR_HANDLE multiMateTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4 + i);
		D3D12_GPU_DESCRIPTOR_HANDLE multiMateTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4 + i);

		// SRVを作成
		device->CreateShaderResourceView(multiMateTextureResource.Get(), &multiMateSrvDesc, multiMateTextureSrvHandleCPU);

		textureSrvHandleCPUs.push_back(multiMateTextureSrvHandleCPU);
		textureSrvHandleGPUs.push_back(multiMateTextureSrvHandleGPU);
	}

	// ---------------------------------------------------Texture---------------------------------------------------//


	//------------------------------------------------------Sprite------------------------------------------------------
	// Sprite用の頂点リソースを作成//
	ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device.Get(), sizeof(VertexData) * 6);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress(); // リソースの先頭アドレスから使う
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6; // 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData); // 一つの頂点のサイズ

	// 頂点リソースにデータを書き込む
	VertexData* vertexDataSprite = nullptr;
	// Map
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// 一つ目の三角形
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f, 0.0f, -1.0f };

	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };

	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };

	// 二つ目の三角形
	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左下
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };

	vertexDataSprite[4].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 右上
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[4].normal = { 0.0f, 0.0f, -1.0f };

	vertexDataSprite[5].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[5].normal = { 0.0f, 0.0f, -1.0f };

	// Sprite用のVertexIndexを作成//
	ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device.Get(), sizeof(uint32_t) * 6);

	// IndexBufferView
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress(); // リソースの先頭アドレスから使う
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6; // 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT; // インデックスのフォーマット

	// Indexリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;

	// Map
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));

	// 三角形のインデックスデータを作成
	indexDataSprite[0] = 0;	indexDataSprite[1] = 1;	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;	indexDataSprite[4] = 4;	indexDataSprite[5] = 2;

	// Sprite用のTrasformationMatrixCBufferリソースを作成//
	ComPtr<ID3D12Resource> TrasformationMatrixResourceSprite = CreateBufferResource(device.Get(), sizeof(TransformationMatrix));

	// データを書き込む
	TransformationMatrix* TrasformationMatrixDataSprite = nullptr;
	// Map
	TrasformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&TrasformationMatrixDataSprite));
	// 単位行列を書き込んでいく
	TrasformationMatrixDataSprite->WVP = MakeIdentityMatrix4x4();
	TrasformationMatrixDataSprite->world = MakeIdentityMatrix4x4();

	Transform transformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// Sprite用のwvpMatrix
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentityMatrix4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthoMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
	Matrix4x4 wvpMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

	TrasformationMatrixDataSprite->WVP = wvpMatrixSprite;
	TrasformationMatrixDataSprite->world = worldMatrixSprite;

	// Sprite用のMaterialリソースを作成//
	ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device.Get(), sizeof(Material));

	// マテリアルにデータを書き込む
	Material* materialDataSprite = nullptr;
	// Map
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	// 白
	materialDataSprite[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// ライティングを無効にする
	materialDataSprite[0].enableLighting = false;
	// uvTransformを初期化
	materialDataSprite[0].uvTransform = MakeIdentityMatrix4x4();

	Transform uvTransformSprite{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};

	//------------------------------------------------------Sprite------------------------------------------------------//

	//-------------------------------------------------------Light-------------------------------------------------------
	// 平行光源のリソースを作成
	ComPtr<ID3D12Resource> lightResource = CreateBufferResource(device.Get(), sizeof(DirectionalLight));

	// ライトにデータを書き込む
	DirectionalLight* lightData = nullptr;
	// Map
	lightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightData));
	// ライトの方向
	lightData[0].direction = Vector3(0.0f, -1.0f, 0.0f);
	// ライトの色
	lightData[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// ライトのタイプ
	// 0:Lambert 1:Half-Lambert
	lightData[0].lightType = 0;

	// 輝度
	lightData[0].intensity = 1.0f;
	//-------------------------------------------------------Light-------------------------------------------------------//

	// DepthStencilResourceの作成---------------------------------------------------------------
	ComPtr<ID3D12Resource> depthStencilResource = CreateDepthStencilResource(device.Get(), kClientWidth, kClientHeight);
	//ID3D12Resource* depthStencilResource = CeateDepthStencilResource(device, kClientWidth, kClientHeight);

	// DSVの作成、descriptorの数は1、shader内で触るものではないのでShaderVisibleはfalse
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	//ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, GetCPUDescriptorHandle(dsvDescriptorHeap.Get(), descriptorSizeDSV, 0));

	// DepthStencilResourceの作成---------------------------------------------------------------//


	// viewPortの設定
	D3D12_VIEWPORT viewPort{};
	viewPort.Width = kClientWidth;
	viewPort.Height = kClientHeight;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;

	// scissorRectの設定
	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	//-----------------------------------------DirectX-----------------------------------------//

	//-----------------------------------------XAudio2-----------------------------------------//
	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	// XAudio2の初期化
	hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	hr = xAudio2->CreateMasteringVoice(&masterVoice);

	// サウンドの読み込み
	SoundData soundData = LoadWaveFile("resources/fanfare.wav");

	//-----------------------------------------XAudio2-----------------------------------------//


	//-----------------------------------------Imgui-----------------------------------------//
	//ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX12_Init(
		device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	//-----------------------------------------Imgui-----------------------------------------//



	//---------------------------------------------------GAMELOOP-----------------------------------------------------//
	MSG msg{};

	while (msg.message != WM_QUIT)
	{
		//メッセージがある場合は処理
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

		} else {
			//-------------imguiの初期化-------------//
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			//-------------imguiの初期化-------------//

			/// <summary>
			/// 更新処理
			/// </summary>

			// modelの座標変換
			Matrix4x4 worldMatrix = MakeAffineMatrix(modelTransform.scale, modelTransform.rotate, modelTransform.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveMatrix(0.45f, static_cast<float>(kClientWidth) / static_cast<float>(kClientHeight), 0.1f, 100.0f);
			Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

			// modelのWVPにデータを書き込む
			modelTrasformationMatrixData->WVP = wvpMatrix;
			modelTrasformationMatrixData->world = worldMatrix;

			// Sphereの座標変換
			Matrix4x4 worldMatrixSphere = MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
			Matrix4x4 wvpMatrixSphere = Multiply(worldMatrixSphere, Multiply(viewMatrix, projectionMatrix));

			sphereTrasformationMatrixData->WVP = wvpMatrixSphere;
			sphereTrasformationMatrixData->world = worldMatrixSphere;

			// modelのuvTransform
			Matrix4x4 uvTransformMatrixModel = MakeAffineMatrix(uvTransformModel.scale, uvTransformModel.rotate, uvTransformModel.translate);
			materialData[0].uvTransform = uvTransformMatrixModel;

			// Spriteの座標変換
			worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			viewMatrixSprite = Inverse(cameraMatrix);
			wvpMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

			TrasformationMatrixDataSprite->WVP = wvpMatrixSprite;
			TrasformationMatrixDataSprite->world = worldMatrixSprite;

			// SpriteのuvTransform
			Matrix4x4 uvTransformMatrix = MakeAffineMatrix(uvTransformSprite.scale, uvTransformSprite.rotate, uvTransformSprite.translate);
			materialDataSprite[0].uvTransform = uvTransformMatrix;

			// バックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// ---PRESENTからRTV用のStateにするTransitionBarrierを張る--- //
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			// 遷移前（現在）のresourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のresourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// バリアを張る
			commandList->ResourceBarrier(1, &barrier);
			// ---PRESENTからRTV用のStateにするTransitionBarrierを張る--- //

			// imgui描画用のDescriptorHeapを設定。ClearRenderTargetViewの後、実際の描画する前に設定する
			ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap.Get() };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);

			// DepthStencilのクリア
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandle[backBufferIndex], false, &dsvHandle);

			//-------------------ImGui-------------------//
			ImGui::Begin("Option");

			if (ImGui::BeginTabBar("Option"))
			{
				if (ImGui::BeginTabItem("obj Model"))
				{
					const char* ModelType_items[] = { "Plane", "teapot", "bunny", "MultiMesh", "MultiMaterial", "Suzanne" };
					static int ModelType_item_current = 0;
					ImGui::Combo("ModelType", &ModelType_item_current, ModelType_items, IM_ARRAYSIZE(ModelType_items));
					if (ImGui::Button("Load"))
					{
						if (ModelType_item_current == 0)
						{
							modelType = Plane;
						} else if (ModelType_item_current == 1)
						{
							modelType = Teapot;
						} else if (ModelType_item_current == 2)
						{
							modelType = Bunny;
						} else if (ModelType_item_current == 3)
						{
							modelType = MultiMesh;
						} else if (ModelType_item_current == 4)
						{
							modelType = MultiMaterial;
						} else if (ModelType_item_current == 5)
						{
							modelType = Suzanne;
						}
					}

					ImGui::Separator();

					ImGui::DragFloat3("Scale", &modelTransform.scale.x, 0.1f, 0.0f, 50.0f);
					ImGui::DragFloat3("Rotate", &modelTransform.rotate.x, 0.1f, 0.0f, 6.28f);
					ImGui::DragFloat3("Translate", &modelTransform.translate.x, 0.1f, -50.0f, 50.0f);
					ImGui::ColorEdit4("Color", &materialData[0].color.x);

					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Sphere"))
				{
					ImGui::DragFloat3("Scale", &sphereTransform.scale.x, 0.1f, 0.0f, 50.0f);
					ImGui::DragFloat3("Rotate", &sphereTransform.rotate.x, 0.1f, 0.0f, 6.28f);
					ImGui::DragFloat3("Translate", &sphereTransform.translate.x, 0.1f, -50.0f, 50.0f);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Sprite"))
				{
					ImGui::DragFloat3("Scale", &transformSprite.scale.x, 0.1f, 0.0f, 50.0f);
					ImGui::DragFloat3("Rotate", &transformSprite.rotate.x, 0.1f, 0.0f, 6.28f);
					ImGui::DragFloat3("Translate", &transformSprite.translate.x, 1.0f, -1000.0f, 1000.0f);
					ImGui::ColorEdit4("Color", &materialDataSprite[0].color.x);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Light"))
				{
					ImGui::Checkbox("EnableLighting", &materialData[0].enableLighting);
					ImGui::Separator();
					ImGui::DragFloat3("Direction", &lightData[0].direction.x, 0.1f, -1.0f, 1.0f);
					ImGui::ColorEdit4("Color", &lightData[0].color.x);
					ImGui::DragFloat("Intensity", &lightData[0].intensity, 0.1f, 0.0f, 10.0f);
					ImGui::Separator();
					const char* LightType_items[] = { "Lambert", "Half-Lambert" };
					static int LightType_item_current = 0;
					ImGui::Combo("LightType", &LightType_item_current, LightType_items, IM_ARRAYSIZE(LightType_items));
					lightData[0].lightType = LightType_item_current;
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Sound"))
				{
					if (ImGui::Button("Play"))
					{
						SoundPlay(xAudio2.Get(), soundData);
					}
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::End();

			ImGui::Begin("UVTransform");
			if (ImGui::BeginTabBar("Option"))
			{
				if (ImGui::BeginTabItem("Sprite"))
				{
					ImGui::DragFloat2("Scale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
					ImGui::DragFloat2("Translate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
					ImGui::SliderAngle("Rotate", &uvTransformSprite.rotate.z);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Model"))
				{
					ImGui::DragFloat2("Scale", &uvTransformModel.scale.x, 0.01f, -10.0f, 10.0f);
					ImGui::DragFloat2("Translate", &uvTransformModel.translate.x, 0.01f, -10.0f, 10.0f);
					ImGui::SliderAngle("Rotate", &uvTransformModel.rotate.z);
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::End();

			// ImGuiの内部コマンドを生成。描画処理の前に行う
			ImGui::Render();

			//-------------------ImGui-------------------//

			/// <summary>
			/// 描画処理
			/// </summary>
			
			// クリアカラー
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

			// 画面をクリア
			commandList->ClearRenderTargetView(rtvHandle[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			commandList->RSSetViewports(1, &viewPort);
			commandList->RSSetScissorRects(1, &scissorRect);

			commandList->SetGraphicsRootSignature(rootSignature.Get()); // ルートシグネチャの設定
			commandList->SetPipelineState(graphicsPipelineState.Get()); // パイプラインステートの設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // トポロジの設定

			//-----------Modelの描画-----------//
			// マテリアルの設定。色を変える
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress()); // マテリアルCBufferの場所を設定

			// WVPのcBufferの設定
			commandList->SetGraphicsRootConstantBufferView(1, modelWvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

			// Lightの設定
			commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());

			if (modelType == Plane)
			{
				// Textureの設定
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);

				// 頂点バッファの設定
				commandList->IASetVertexBuffers(0, 1, &planeVertexBufferView);

				// 描画
				commandList->DrawInstanced(UINT(planeData.vertices.size()), 1, 0, 0);
			} else if (modelType == Teapot)
			{
				// Textureの設定
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU3);

				// 頂点バッファの設定
				commandList->IASetVertexBuffers(0, 1, &teapotVertexBufferView);

				// 描画
				commandList->DrawInstanced(UINT(teapotData.vertices.size()), 1, 0, 0);
			} else if (modelType == Bunny)
			{
				// Textureの設定
				commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

				// 頂点バッファの設定
				commandList->IASetVertexBuffers(0, 1, &bunnyVertexBufferView);

				// 描画
				commandList->DrawInstanced(UINT(bunnyData.vertices.size()), 1, 0, 0);
			} else if (modelType == MultiMesh) {
				for (int i = 0; i < multiMeshModelDatas.size(); i++)
				{
					// Textureの設定
					commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

					// 頂点バッファの設定
					commandList->IASetVertexBuffers(0, 1, &multiMeshVertexBufferViews[i]);

					// 描画
					commandList->DrawInstanced(UINT(multiMeshModelDatas[i].vertices.size()), 1, 0, 0);
				}
			} else if (modelType == MultiMaterial) {
				for (int i = 0; i < multiMaterialModelDatas.size(); i++)
				{
					// Textureの設定
					commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPUs[i]);

					// 頂点バッファの設定
					commandList->IASetVertexBuffers(0, 1, &multiMaterialVertexBufferViews[i]);

					// 描画
					commandList->DrawInstanced(UINT(multiMaterialModelDatas[i].vertices.size()), 1, 0, 0);
				}
			}

			//-----------Modelの描画-----------//

			//-----------Sphereの描画-----------//
			// WVPのcBufferの設定
			commandList->SetGraphicsRootConstantBufferView(1, sphereWvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

			// Textureの設定
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// 頂点バッファの設定
			commandList->IASetVertexBuffers(0, 1, &sphereVertexBufferView);

			// インデックスバッファの設定
			commandList->IASetIndexBuffer(&sphereIndexBufferView);

			// 描画
			commandList->DrawIndexedInstanced(kVertexCount, 1, 0, 0, 0);
			//-----------Sphereの描画-----------//


			//-----------Spriteの描画-----------//
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress()); // マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(1, TrasformationMatrixResourceSprite->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU); // Textureの設定
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // 頂点バッファの設定
			commandList->IASetIndexBuffer(&indexBufferViewSprite); // インデックスバッファの設定
			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // 描画
			//-----------Spriteの描画-----------//

			//-----------Suzanneの描画-----------//
			if (modelType == Suzanne)
			{
				// ルートシグネチャの設定
				commandList->SetGraphicsRootSignature(rootSignatureNoTex.Get());

				// PSOの設定
				commandList->SetPipelineState(graphicsPipelineStateNoTex.Get());

				// マテリアルの設定。色を変える
				commandList->SetGraphicsRootConstantBufferView(1, materialResource->GetGPUVirtualAddress()); // マテリアルCBufferの場所を設定

				// WVPのcBufferの設定
				commandList->SetGraphicsRootConstantBufferView(0, modelWvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

				// Lightの設定
				commandList->SetGraphicsRootConstantBufferView(2, lightResource->GetGPUVirtualAddress()); // LightのCBufferの場所を設定

				// 頂点バッファの設定
				commandList->IASetVertexBuffers(0, 1, &suzanneVertexBufferView);

				// 描画
				commandList->DrawInstanced(UINT(suzanneData.vertices.size()), 1, 0, 0);
			}
			//-----------Suzanneの描画-----------//


			// commandListにimguiの描画コマンドを積む。描画処理の後、RTVからPRESENT Stateに戻す前に行う
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());


			// ---RTVからPRESENTStateに戻すTransitionBarrierを張る--- //
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);
			// ---RTVからPRESENTStateに戻すTransitionBarrierを張る--- //

			// コマンドリストを閉じる
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, commandLists);

			// GPUとOSに画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			//Fenceを使ってGPUの処理が終わるのを待つ
			fenceValue++;
			commandQueue->Signal(fence.Get(), fenceValue);

			if (fence->GetCompletedValue() < fenceValue)
			{
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));
		}
	}

	//-------------------------------------------------GAMELOOP-----------------------------------------------------/

	CoUninitialize();

	// 解放
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// リソースの解放
	dxcUtils->Release();
	dxcCompiler->Release();
	vertexShaderBlob->Release();
	pixelShaderBlob->Release();
	includeHandler->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();

	CloseHandle(fenceEvent);

	// XAudio2の解放
	xAudio2.Reset();
	SoundUnload(&soundData);

#ifdef _DEBUG
	debugController->Release();
#endif 
	CloseWindow(hWnd);

	return 0;
}


// 関数の定義-------------------------------------------------------------------------------------------------------------------
void Log(const std::string& messege)
{
	OutputDebugStringA(messege.c_str());
}

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

//CompileShader
IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	//hlslファイルを読み込む
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, prefile:{}\n", filePath, profile)));
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	//コンパイルオプション
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi", //デバッグ情報を出力
		L"-Qembed_debug", //デバッグ情報を埋め込む
		L"-Od", //最適化なし
		L"-Zpr", //行数情報を出力
	};

	//コンパイル
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments, _countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	//エラーメッセージを取得
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Log(shaderError->GetStringPointer());
		assert(false);
	}

	//コンパイル結果を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, prefile:{}\n", filePath, profile)));
	shaderSource->Release();
	shaderResult->Release();
	return shaderBlob;

}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes)
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
	ID3D12Resource* bufferResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferResource));
	assert(SUCCEEDED(hr));
	return bufferResource;
}

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	// ヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = heapType;
	heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// ヒープの生成
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	// テクスチャの読み込み
	DirectX::ScratchImage image;
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// mipmapを生成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	return mipImages;

}

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metaData)
{
	// テクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metaData.width); // テクスチャの幅
	resourceDesc.Height = UINT(metaData.height); // テクスチャの高さ
	resourceDesc.DepthOrArraySize = UINT16(metaData.arraySize); // 配列サイズ
	resourceDesc.MipLevels = UINT16(metaData.mipLevels); // ミップマップレベル
	resourceDesc.Format = metaData.format; // フォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプル数
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metaData.dimension); // テクスチャの次元

	// ヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// Resourceの設定
	ID3D12Resource* textureResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // ヒープの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // リソースの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // リソースの初期状態. データ転送される設定
		nullptr, // クリア値の設定。今回は使わないのでnullptr
		IID_PPV_ARGS(&textureResource)); // 生成したリソースのpointerへのpointerを取得
	assert(SUCCEEDED(hr));

	return textureResource;

}

[[nodiscard]]
ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	// subresourcesの配列を作る
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	// intermediateResourceに必要なサイズを取得
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	// intermediateResourceを作成
	ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);

	// intermediateResourceにSubresourceのデータを書き込み、textureに転送するコマンドを積む
	UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

	// ResourceBarrierを使ってResourceStateを変更
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}


ID3D12Resource* CreateDepthStencilResource(ID3D12Device* device, int32_t width, int32_t height)
{
	// テクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width; // テクスチャの幅
	resourceDesc.Height = height; // テクスチャの高さ
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

	// Resourceの設定
	ID3D12Resource* depthStencilResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // ヒープの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // リソースの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // リソースの初期状態. DEPTH_WRITE
		&depthClearValue, // クリア値の設定.
		IID_PPV_ARGS(&depthStencilResource)); // 生成したリソースのpointerへのpointerを取得
	assert(SUCCEEDED(hr));

	return depthStencilResource;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (descriptorSize * index);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += (descriptorSize * index);
	return handle;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& fileName)
{
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefiniton;
				s >> vertexDefiniton;

				std::istringstream v(vertexDefiniton);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };

			}

			// 三角形の頂点データを追加
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			modelData.material = LoadMtlFile(directoryPath, mtlFileName);
		}
	}

	return modelData;
}

MaterialData LoadMtlFile(const std::string& directoryPath, const std::string& fileName)
{
	MaterialData materialData;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFileName;
			s >> textureFileName;
			materialData.texturePath = directoryPath + "/" + textureFileName;
		}
	}

	return materialData;
}

std::vector<ModelData> LoadMutiMeshObjFile(const std::string& directoryPath, const std::string& fileName)
{
	std::vector<ModelData> modelDatas;
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };

			}

			// Add the triangle vertices in reverse order
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "o" || identifier == "g") {
			if (!modelData.vertices.empty()) {
				modelDatas.push_back(modelData);
				modelData = ModelData();
			}
		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			modelData.material = LoadMtlFile(directoryPath, mtlFileName);
		}
	}

	if (!modelData.vertices.empty()) {
		modelDatas.push_back(modelData);
	}

	return modelDatas;
}

std::vector<ModelData> LoadMutiMaterialFile(const std::string& directoryPath, const std::string& fileName)
{
	std::vector<ModelData> modelDatas;
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::unordered_map<std::string, MaterialData> materials;
	std::string currentMaterial;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };
			}

			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "o" || identifier == "g") {
			if (!modelData.vertices.empty()) {
				modelDatas.push_back(modelData);
				modelData = ModelData();
			}
		} else if (identifier == "usemtl") {
			s >> currentMaterial;
			if (!modelData.vertices.empty()) {
				modelDatas.push_back(modelData);
				modelData = ModelData();
			}
			modelData.material = materials[currentMaterial];
		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			materials = LoadMutiMaterialMtlFile(directoryPath, mtlFileName);
		}
	}

	if (!modelData.vertices.empty()) {
		modelDatas.push_back(modelData);
	}

	return modelDatas;
}

std::unordered_map<std::string, MaterialData> LoadMutiMaterialMtlFile(const std::string& directoryPath, const std::string& fileName)
{
	std::unordered_map<std::string, MaterialData> materials;
	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	std::string line, currentMaterialName;

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "newmtl") {
			s >> currentMaterialName;
			materials[currentMaterialName] = MaterialData(); // マテリアルを初期化
		} else if (identifier == "map_Kd") {
			std::string textureFileName;
			s >> textureFileName;
			materials[currentMaterialName].texturePath = directoryPath + "/" + textureFileName;
		}
	}

	return materials;
}

ModelDataNoTex LoadObjFileNoTex(const std::string& directoryPath, const std::string& fileName) {
	ModelDataNoTex modelData;
	VertexDataNoTex triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				// 文字列を二つのスラッシュ "//" で分割する
				size_t firstSlash = vertexDefinition.find("//");
				size_t secondSlash = vertexDefinition.find("//", firstSlash + 2);

				// 頂点位置と法線のインデックスを抽出する
				uint32_t positionIndex = std::stoi(vertexDefinition.substr(0, firstSlash));
				uint32_t normalIndex = std::stoi(vertexDefinition.substr(firstSlash + 2, secondSlash - (firstSlash + 2)));

				Vector4 position = positions[positionIndex - 1];
				Vector3 normal = normals[normalIndex - 1];

				// 座標系変換を処理する
				position.z *= -1.0f;
				normal.z *= -1.0f;

				triangleVertices[facevertex] = { position, normal };
			}

			// 三角形の頂点データを追加する
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);
		}
	}

	return modelData;
}


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


	FormatChunk format= {};
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