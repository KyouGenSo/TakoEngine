#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include<wrl.h>
#include "WinApp.h"

class DX12Basic {
public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);

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
	/// デスクリプタヒープの生成
	/// <summary>+
	ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

private: // プライベート関数


private: // メンバ変数
	// ウィンドウクラスポインター
	WinApp* winApp_ = nullptr;

	// デバイス
	ComPtr<ID3D12Device> device_;

	// DXGIファクトリ
	ComPtr<IDXGIFactory7> dxgiFactory;

	// コマンドキュー
	ComPtr<ID3D12CommandQueue> commandQueue_;

	// コマンドアロケータ
	ComPtr<ID3D12CommandAllocator> commandAllocator_;

	// コマンドリスト
	ComPtr<ID3D12GraphicsCommandList> commandList_;

	// スワップチェイン
	ComPtr<IDXGISwapChain4> swapChain_;

	// 深度バッファ
	ComPtr<ID3D12Resource> depthStencilResource_;

	// デスクリプタヒープのサイズ
	uint32_t descriptorSizeRTV_;
	uint32_t descriptorSizeDSV_;
	uint32_t descriptorSizeSRV_;

	// レンダーターゲットビューのデスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> rtvHeap_;

	// 深度ステンシルビューのデスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> dsvHeap_;

	// シェーダーリソースビューのデスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
};
