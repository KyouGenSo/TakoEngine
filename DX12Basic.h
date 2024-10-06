#pragma once
#include "WinApp.h"
#include <array>
#include<string>
#include<fstream>
#include<sstream>
#include<chrono>
#include<wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include<dxcapi.h>
#include"externals/DirectXTex/DirectXTex.h"

class DX12Basic {
public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>
	/// デストラクタ
	/// </summary>
	~DX12Basic();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);

	/// <summary>
	/// 描画前の処理
	/// </summary> 
	void BeginDraw();

	/// <summary>
	/// 描画後の処理
	/// </summary>
	void EndDraw();

	/// <summary>
	/// SRVの指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCpuDescriptorHandle(uint32_t index);

	/// <summary>
	/// SRVの指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpuDescriptorHandle(uint32_t index);

	/// <summary>
	/// コンパイルシェーダー
	/// </summary>
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);

	/// <summary>
	/// バッファリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/// <summary>
	/// テクスチャリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metaData);

	/// <summary>
	/// テクスチャリソースの転送
	/// </summary>
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	//-----------------------------------------getter-----------------------------------------//
	/// <summary>
	/// デバイスの取得
	/// </summary>
	ID3D12Device* GetDevice() {
		return device_.Get();
	}

	/// <summary>
	/// コマンドリストの取得
	/// </summary>
	ID3D12GraphicsCommandList* GetCommandList() {
		return commandList_.Get();
	}

private: // プライベートメンバー関数
	/// <summary>
	/// deviceの初期化
	/// </summary>
	void InitDevice();

	/// <summary>
	/// コマンド関連の初期化
	/// </summary>
	void InitCommand();

	/// <summary>
	/// スワップチェインの生成
	/// </summary>
	void CreateSwapChain();

	/// <summary>
	/// 深度バッファの生成
	/// </summary>
	void CreateDepthStencilResource();

	/// <summary>
	/// デスクリプタヒープの初期化
	/// <summary>
	void InitDescriptorHeap();

	/// <summary>
	/// レンダーターゲットビューの初期化
	/// <summary> 
	void InitRTV();

	/// <summary>
	/// 深度ステンシルビューの初期化
	/// <summary> 
	void InitDSV();

	/// <summary>
	/// フェンスの初期化
	/// <summary> 
	void InitFence();

	/// <summary>
	///　ビューポート矩形の初期化
	/// <summary> 
	void InitViewport();

	/// <summary>
	/// シザリング矩形の初期化
	/// <summary> 
	void InitScissorRect();

	/// <summary>
	/// DXCコンパイラの生成
	/// <summary> 
	void CreateDXCCompiler();

	/// <summary>
	/// ImGuiの初期化
	/// <summary> 
	void InitImGui();

	/// <summary>
	/// FPS制御初期化
	/// <summary> 
	void InitFPSLimiter();

	/// <summary>
	/// FPS制御更新
	/// <summary> 
	void UpdateFPSLimiter();

	/// <summary>
	/// デスクリプタヒープの生成
	/// <summary>+
	ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// 指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// 指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

private: // メンバ変数
	
	// 記録時間(FPS制御用)
	std::chrono::steady_clock::time_point referenceTime_;

	// ウィンドウクラスポインター
	WinApp* winApp_ = nullptr;

	// デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;

	// DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;

	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

	// スワップチェイン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

	// 深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	// デスクリプタヒープのサイズ
	uint32_t descriptorSizeRTV_;
	uint32_t descriptorSizeDSV_;
	uint32_t descriptorSizeSRV_;

	// レンダーターゲットビューのデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;

	// 深度ステンシルビューのデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;

	// シェーダーリソースビューのデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

	// スワップチェインのバッファ
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;

	// スワップチェインのバッファのカウント
	UINT swapChainBufferCount_;

	// RTVハンドルの要素数
	static const UINT kRtvHandleCount_ = 2;

	// RTVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[kRtvHandleCount_];

	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	// フェンスイベント
	HANDLE fenceEvent_;

	// フェンスの値
	UINT64 fenceValue_;

	// ビューポート
	D3D12_VIEWPORT viewport_;

	// シザリング矩形
	D3D12_RECT scissorRect_;

	// DXCUtility
	IDxcUtils* dxcUtils_ = nullptr;

	// DXCコンパイラ
	IDxcCompiler3* dxcCompiler_ = nullptr;

	// デフォルトインクルードハンドラー
	IDxcIncludeHandler* includeHandler_ = nullptr;

};
