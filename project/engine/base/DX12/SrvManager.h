#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <iostream>

class DX12Basic;

class SrvManager {
private: // シングルトン設定

	// インスタンス
	static SrvManager* instance_;

	SrvManager() = default;
	~SrvManager() = default;
	SrvManager(SrvManager&) = delete;
	SrvManager& operator=(SrvManager&) = delete;

public: // メンバー関数

	// 最大SRV数(テクスチャ数)
	static const uint32_t kMaxSRVCount;

	///<summary>
	/// インスタンスの取得
	/// </summary>
	static SrvManager* GetInstance();

	///<summary>
	///初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	///<summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 描画前の処理
	/// </summary> 
	void BeginDraw();

	///<summary>
	///SRVの確保
	/// </summary>
	uint32_t Allocate();

	///<summary>
	///確保可能チェック
	/// </summary>
	bool CanAllocate();

	///<summary>
	///SRV生成(テクスチャ用)
	/// </summary>
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

	///<summary>
	///SRV生成(StructuredBuffer用)
	/// </summary>
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	///<summary>
	///RootDescriptorTableにSRVをセット
	/// </summary>
	void SetRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

	/// <summary>
	/// 指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// 指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private: // メンバー変数

	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	// SRVのディスクリプタのサイズ
	uint32_t descriptorSize_;

	// SRVのディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	// 次に使用するsrvのインデックス
	uint32_t srvIndex = 0;
};