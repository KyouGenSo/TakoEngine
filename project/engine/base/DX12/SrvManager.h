#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <iostream>
#include <queue>
#include <unordered_set>

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
  ///SRVの解放
  /// </summary>
  void Free(uint32_t index);

	///<summary>
	///確保可能チェック
	/// </summary>
	bool CanAllocate();

  ///<summary>
  ///使用中かどうかチェック
  /// </summary>
  bool IsAllocated(uint32_t index) const;

  ///<summary>
  ///使用中のSRV数を取得
  /// </summary>
  uint32_t GetAllocatedCount() const { return allocatedCount_; }

	///<summary>
	///SRV生成(テクスチャ用)
	/// </summary>
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

	///<summary>
	///SRV生成(StructuredBuffer用)
	/// </summary>
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

  ///<summary>
  ///UAV生成(ComputeShader用)
  /// </summary>
  void CreateUAV(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

  ///<summary>
  ///　SRV生成(CubeMap用)
  /// </summary>
  void CreateSRVForCubeMap(uint32_t _srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

	///<summary>
	///GraphicsRootDescriptorTableにSRVをセット
	/// </summary>
	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

  ///<summary>
  ///ComputeRootDescriptorTableにSRVをセット
  /// </summary>
  void SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t index);

	/// <summary>
	/// 指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// 指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// ディスクリプタヒープを取得
	/// </summary>
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

private: // メンバー変数

	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	// SRVのディスクリプタのサイズ
	uint32_t descriptorSize_;

	// SRVのディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

  // 解放されたインデックスを管理するキュー（フリーリスト）
  std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> freeIndices_;

  // 使用中のインデックスを管理するセット
  std::unordered_set<uint32_t> usedIndices_;

  // 次に使用する新しいインデックス（フリーリストが空の場合に使用）
  uint32_t nextNewIndex_ = 0;

  // 現在使用中のSRV数
  uint32_t allocatedCount_ = 0;

	// 次に使用するsrvのインデックス
	//uint32_t srvIndex = 0;
};