#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include<wrl.h>
#include "WinApp.h"
#include <array>
#include<dxcapi.h>

class DX12Basic {
public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);

	/// <summary>
	/// SRVの指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCpuDescriptorHandle(uint32_t index);
 
	/// <summary>
	/// SRVの指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpuDescriptorHandle(uint32_t index);

private: // プライベート関数
	/// <summary>
	/// deviceの初期化
	/// </summary>
	void DeviceInit();

	/// <summary>
	/// コマンド関連の初期化
	/// </summary>
	void CommandInit();

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
	void DescriptorHeapInit();

	/// <summary>
	/// レンダーターゲットビューの初期化
	/// <summary> 
	void RTVInit();

	/// <summary>
	/// 深度ステンシルビューの初期化
	/// <summary> 
	void DSVInit();

	/// <summary>
	/// フェンスの初期化
	/// <summary> 
	void FenceInit();

	/// <summary>
	///　ビューポート矩形の初期化
	/// <summary> 
	void ViewportInit();

	/// <summary>
	/// シザリング矩形の初期化
	/// <summary> 
	void ScissorRectInit();

	/// <summary>
	/// DXCコンパイラの生成
	/// <summary> 
	void CreateDXCCompiler();

	/// <summary>
	/// ImGuiの初期化
	/// <summary> 
	void ImGuiInit();

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

	// RTVハンドルの要素数
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[kRtvHandleCount_];

	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	// フェンスイベント
	HANDLE fenceEvent_;

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
