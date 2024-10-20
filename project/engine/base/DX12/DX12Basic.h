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

#include"Vector4.h"

class DX12Basic {
public: // メンバー関数

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 最大SRV数(テクスチャ数)
	static const uint32_t kMaxSRVCount;

	/// <summary>
	/// デストラクタ
	/// </summary>
	~DX12Basic();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// renderTextureを設定
	/// </summary> 
	void SetRenderTexture();

	/// <summary>
	/// swapChainを設定
	/// </summary> 
	void SetSwapChain();

	/// <summary>
	/// 描画後の処理
	/// </summary>
	void EndDraw();

	/// <summary>
	/// // コマンド完了まで待機
	/// </summary>
	void WaitForGPU();

	/// <summary>
	/// コンパイルシェーダー
	/// </summary>
	ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);

	/// <summary>
	/// バッファリソースの生成
	/// </summary>
	ComPtr<ID3D12Resource> MakeBufferResource(size_t sizeInBytes);
	void CreateBufferResource(ComPtr<ID3D12Resource>& bufferResource, size_t sizeInBytes);

	/// <summary>
	/// テクスチャリソースの生成
	/// </summary>
	ComPtr<ID3D12Resource> MakeTextureResource(const DirectX::TexMetadata& metaData);
	void CreateTextureResource(ComPtr<ID3D12Resource>& textureResource, const DirectX::TexMetadata& metaData);

	/// <summary>
	/// レンダーテクスチャリソースの生成
	/// </summary>
	void CreateRenderTextureResource(ComPtr<ID3D12Resource>& rendertextureResource, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	/// <summary>
	/// デスクリプタヒープの生成
	/// <summary>+
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// テクスチャリソースの転送
	/// </summary>
	[[nodiscard]]
	ComPtr<ID3D12Resource> UploadTextureData(const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	//-----------------------------------------Getter-----------------------------------------//
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

	/// <summary>
	/// コマンドキューの取得
	/// </summary>
	ID3D12CommandQueue* GetCommandQueue() {
		return commandQueue_.Get();
	}

	/// <summary>
	/// backBufferの数の取得
	/// </summary>
	size_t GetSwapChainBufferCount() {
		return swapChainResources_.size();
	}

	/// <summary>
	/// レンダーテクスチャのcpuハンドルの取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTextureRTVHandle() {
		return GetCPUDescriptorHandle(rtvHeap_.Get(), descriptorSizeRTV_, 2);
	}

	/// <summary>
	/// DSVHeapの先頭のハンドルの取得
	///	</summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHeapHandleStart() {
		return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
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
	/// FPS制御初期化
	/// <summary> 
	void InitFPSLimiter();

	/// <summary>
	/// FPS制御更新
	/// <summary> 
	void UpdateFPSLimiter();

	/// <summary>
	/// 指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// 指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// バリアの設定
	/// </summary>
	void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);


private: // メンバ変数

	// 記録時間(FPS制御用)
	std::chrono::steady_clock::time_point referenceTime_;

	// RTVハンドルの要素数
	static const UINT kRtvHandleCount = 2;

	// ウィンドウクラスポインター
	WinApp* winApp_ = nullptr;

	// デバイス
	ComPtr<ID3D12Device> device_;

	// DXGIファクトリ
	ComPtr<IDXGIFactory7> dxgiFactory_;

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

	// レンダーターゲットビューのデスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> rtvHeap_;

	// 深度ステンシルビューのデスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> dsvHeap_;

	// スワップチェインのバッファ
	std::array<ComPtr<ID3D12Resource>, 2> swapChainResources_;

	// スワップチェインのバッファのカウント
	UINT swapChainBufferCount_;

	// RTVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[kRtvHandleCount];

	// フェンス
	ComPtr<ID3D12Fence> fence_;

	// フェンスイベント
	HANDLE fenceEvent_;

	// フェンスの値
	UINT64 fenceValue_;

	// ビューポート
	D3D12_VIEWPORT viewport_;

	// シザリング矩形
	D3D12_RECT scissorRect_;

	// DXCUtility
	ComPtr<IDxcUtils> dxcUtils_ = nullptr;

	// DXCコンパイラ
	ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;

	// デフォルトインクルードハンドラー
	ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;

	
	//ComPtr<ID3D12Resource> bufferResource_;

	//ComPtr<ID3D12Resource> textureResource_;

};
