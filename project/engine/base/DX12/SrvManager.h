#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <iostream>
#include <queue>
#include <unordered_set>

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// SRV/UAVディスクリプタ管理クラス
  /// フリーリスト方式によるインデックス管理
  /// </summary>
  class SrvManager {
  private: // シングルトン設定

    static SrvManager* instance_;  ///< シングルトンインスタンス

    SrvManager() = default;
    ~SrvManager() = default;
    SrvManager(SrvManager&) = delete;  ///< コピーコンストラクタを削除
    SrvManager& operator=(SrvManager&) = delete;  ///< 代入演算子を削除

  public: // メンバー関数

    static const uint32_t kMaxSRVCount;  ///< 最大SRV数（テクスチャ数）

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>SrvManagerのシングルトンインスタンス</returns>
    static SrvManager* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX 12基盤システムのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// 描画前の処理
    /// </summary> 
    void BeginDraw();

    /// <summary>
    /// SRVの確保
    /// </summary>
    /// <returns>確保されたSRVのインデックス</returns>
    uint32_t Allocate();

    /// <summary>
    /// SRVの解放
    /// </summary>
    /// <param name="index">解放するSRVのインデックス</param>
    void Free(uint32_t index);

    /// <summary>
    /// 確保可能チェック
    /// </summary>
    /// <returns>確保可能な場合true、不可能な場合false</returns>
    bool CanAllocate();

    /// <summary>
    /// 使用中かどうかチェック
    /// </summary>
    /// <param name="index">チェックするSRVのインデックス</param>
    /// <returns>使用中の場合true、未使用の場合false</returns>
    bool IsAllocated(uint32_t index) const;

    /// <summary>
    /// 使用中のSRV数を取得
    /// </summary>
    /// <returns>使用中のSRV総数</returns>
    uint32_t GetAllocatedCount() const { return allocatedCount_; }

    /// <summary>
    /// SRV生成(テクスチャ用)
    /// </summary>
    /// <param name="srvIndex">SRVを作成するインデックス</param>
    /// <param name="pResource">テクスチャリソース</param>
    /// <param name="format">テクスチャフォーマット</param>
    /// <param name="mipLevels">ミップマップレベル数</param>
    void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

    /// <summary>
    /// SRV生成(StructuredBuffer用)
    /// </summary>
    /// <param name="srvIndex">SRVを作成するインデックス</param>
    /// <param name="pResource">StructuredBufferリソース</param>
    /// <param name="numElements">要素数</param>
    /// <param name="structureByteStride">1要素のバイトサイズ</param>
    void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

    /// <summary>
    /// UAV生成(ComputeShader用)
    /// </summary>
    /// <param name="index">UAVを作成するインデックス</param>
    /// <param name="pResource">UAVリソース</param>
    /// <param name="numElements">要素数</param>
    /// <param name="structureByteStride">1要素のバイトサイズ</param>
    void CreateUAV(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

    /// <summary>
    /// SRV生成(CubeMap用)
    /// </summary>
    /// <param name="_srvIndex">SRVを作成するインデックス</param>
    /// <param name="pResource">CubeMapテクスチャリソース</param>
    /// <param name="format">テクスチャフォーマット</param>
    /// <param name="mipLevels">ミップマップレベル数</param>
    void CreateSRVForCubeMap(uint32_t _srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

    /// <summary>
    /// GraphicsRootDescriptorTableにSRVをセット
    /// </summary>
    /// <param name="rootParameterIndex">ルートパラメータのインデックス</param>
    /// <param name="srvIndex">設定するSRVのインデックス</param>
    void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

    /// <summary>
    /// ComputeRootDescriptorTableにSRVをセット
    /// </summary>
    /// <param name="rootParameterIndex">ルートパラメータのインデックス</param>
    /// <param name="index">設定するSRVのインデックス</param>
    void SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t index);

    /// <summary>
    /// 指定番号のCPUディスクリプタハンドルを取得
    /// </summary>
    /// <param name="index">取得するディスクリプタのインデックス</param>
    /// <returns>CPUディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

    /// <summary>
    /// 指定番号のGPUディスクリプタハンドルを取得
    /// </summary>
    /// <param name="index">取得するディスクリプタのインデックス</param>
    /// <returns>GPUディスクリプタハンドル</returns>
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    /// <summary>
    /// ディスクリプタヒープを取得
    /// </summary>
    /// <returns>SRV/UAV用ディスクリプタヒープ</returns>
    ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

  private: // メンバー変数

    DX12Basic* m_dx12_ = nullptr;  ///< DirectX 12基盤システムへのポインタ

    uint32_t descriptorSize_;  ///< ディスクリプタ1個分のサイズ（バイト単位）

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;  ///< SRV/UAV用ディスクリプタヒープ

    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> freeIndices_;  ///< 解放されたインデックスを小さい順に管理する優先度付きキュー

    std::unordered_set<uint32_t> usedIndices_;  ///< 現在使用中のインデックスを管理するセット（高速な存在チェック用）

    uint32_t nextNewIndex_ = 0;  ///< 次に使用する新しいインデックス（フリーリストが空の場合に使用）

    uint32_t allocatedCount_ = 0;  ///< 現在確保されているSRVの総数

  };

} // namespace Tako