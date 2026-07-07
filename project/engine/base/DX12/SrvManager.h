#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <iostream>
#include <memory>
#include <queue>
#include <unordered_set>

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// SRV/UAV ディスクリプタ管理クラス
  /// フリーリスト方式によるインデックス管理
  /// </summary>
  class SrvManager {
  private: // シングルトン設定
    static std::unique_ptr<SrvManager> instance_;

    SrvManager() = default;
    ~SrvManager() = default;

    friend struct std::default_delete<SrvManager>;

  public:
    SrvManager(const SrvManager&) = delete;
    SrvManager& operator=(const SrvManager&) = delete;

  public: //定数
    static const uint32_t kMaxSRVCount;  ///< 最大 SRV 数（テクスチャ数）

  public: //メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>SrvManager のシングルトンインスタンス</returns>
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
    /// SRV の確保
    /// </summary>
    /// <returns>確保された SRV のインデックス</returns>
    uint32_t Allocate();

    /// <summary>
    /// SRV の解放
    /// </summary>
    /// <param name="index">解放する SRV のインデックス</param>
    void Free(uint32_t index);

    /// <summary>
    /// 確保可能チェック
    /// </summary>
    /// <returns>確保可能な場合 true、不可能な場合 false</returns>
    bool CanAllocate();

    /// <summary>
    /// SRV 生成(テクスチャ用)
    /// </summary>
    /// <param name="srvIndex">SRV を作成するインデックス</param>
    /// <param name="pResource">テクスチャリソース</param>
    /// <param name="format">テクスチャフォーマット</param>
    /// <param name="mipLevels">ミップマップレベル数</param>
    void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

    /// <summary>
    /// SRV 生成(StructuredBuffer 用)
    /// </summary>
    /// <param name="srvIndex">SRV を作成するインデックス</param>
    /// <param name="pResource">StructuredBuffer リソース</param>
    /// <param name="numElements">要素数</param>
    /// <param name="structureByteStride">1要素のバイトサイズ</param>
    void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

    /// <summary>
    /// UAV 生成(ComputeShader 用)
    /// </summary>
    /// <param name="index">UAV を作成するインデックス</param>
    /// <param name="pResource">UAV リソース</param>
    /// <param name="numElements">要素数</param>
    /// <param name="structureByteStride">1要素のバイトサイズ</param>
    void CreateUAV(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

    /// <summary>
    /// SRV 生成(CubeMap 用)
    /// </summary>
    /// <param name="_srvIndex">SRV を作成するインデックス</param>
    /// <param name="pResource">CubeMap テクスチャリソース</param>
    /// <param name="format">テクスチャフォーマット</param>
    /// <param name="mipLevels">ミップマップレベル数</param>
    void CreateSRVForCubeMap(uint32_t _srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);

    /// <summary>
    /// GraphicsRootDescriptorTable に SRV をセット
    /// </summary>
    /// <param name="rootParameterIndex">ルートパラメータのインデックス</param>
    /// <param name="srvIndex">設定する SRV のインデックス</param>
    void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

    /// <summary>
    /// ComputeRootDescriptorTable に SRV をセット
    /// </summary>
    /// <param name="rootParameterIndex">ルートパラメータのインデックス</param>
    /// <param name="index">設定する SRV のインデックス</param>
    void SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t index);

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// 使用中かどうかチェック
    /// </summary>
    /// <param name="index">チェックする SRV のインデックス</param>
    /// <returns>使用中の場合 true、未使用の場合 false</returns>
    bool IsAllocated(uint32_t index) const;

    uint32_t GetAllocatedCount() const { return allocatedCount_; }

    /// <summary>
    /// 指定番号の CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="index">取得するディスクリプタのインデックス</param>
    /// <returns>CPU ディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

    /// <summary>
    /// 指定番号の GPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="index">取得するディスクリプタのインデックス</param>
    /// <returns>GPU ディスクリプタハンドル</returns>
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

  private: //メンバー変数

    DX12Basic* dx12_ = nullptr;  ///< DirectX 12基盤システムへのポインタ

    uint32_t descriptorSize_;  ///< ディスクリプタ1個分のサイズ（バイト単位）

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;  ///< SRV/UAV 用ディスクリプタヒープ

    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> freeIndices_;  ///< 解放されたインデックスを小さい順に管理する優先度付きキュー

    std::unordered_set<uint32_t> usedIndices_;  ///< 現在使用中のインデックスを管理するセット（高速な存在チェック用）

    uint32_t nextNewIndex_ = 1;  ///< 次に使用する新しいインデックス（0 は無効番兵として予約。Initialize 参照）

    uint32_t allocatedCount_ = 0;  ///< 現在確保されている SRV の総数

  };

} // namespace Tako