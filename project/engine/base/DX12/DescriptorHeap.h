#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <queue>
#include <unordered_set>
#include <vector>

namespace Tako {

  /// <summary>
  /// ディスクリプタヒープ1枚とフリーリスト式インデックスアロケータの汎用基盤
  /// </summary>
  class DescriptorHeap {
  public: //定数
    static constexpr uint32_t kInvalidIndex = 0;  ///< 無効番兵。index 0 は予約し割り当てない

  public: //メンバー関数

    /// <summary>
    /// ヒープ生成とアロケータの全リセット
    /// </summary>
    /// <param name="device">D3D12 デバイス</param>
    /// <param name="type">ヒープタイプ（RTV、DSV、CBV_SRV_UAV 等）</param>
    /// <param name="numDescriptors">ディスクリプタ数</param>
    /// <param name="shaderVisible">シェーダーから可視かどうか</param>
    void Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible);

    /// <summary>
    /// ヒープ解放と全状態クリア
    /// </summary>
    void Finalize();

    /// <summary>
    /// インデックスの確保
    /// </summary>
    /// <returns>確保されたインデックス。枯渇時は kInvalidIndex</returns>
    uint32_t Allocate();

    /// <summary>
    /// インデックスの解放。kInvalidIndex とヒープ未初期化時は何もしない
    /// </summary>
    /// <param name="index">解放するインデックス</param>
    void Free(uint32_t index);

    //============================================================
    //Getter
    //============================================================
    bool CanAllocate() const;
    bool IsAllocated(uint32_t index) const;
    bool IsInitialized() const { return heap_ != nullptr; }

    /// <summary>
    /// 指定番号の CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="index">取得するディスクリプタのインデックス</param>
    /// <returns>CPU ディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t index) const;

    /// <summary>
    /// 指定番号の GPU ディスクリプタハンドルを取得（shader visible ヒープのみ）
    /// </summary>
    /// <param name="index">取得するディスクリプタのインデックス</param>
    /// <returns>GPU ディスクリプタハンドル</returns>
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t index) const;

    ID3D12DescriptorHeap* GetHeap() const { return heap_.Get(); }
    uint32_t GetAllocatedCount() const { return allocatedCount_; }
    uint32_t GetCapacity() const { return numDescriptors_; }
    const std::unordered_set<uint32_t>& GetUsedIndices() const { return usedIndices_; }

  private: //メンバー変数

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{};  ///< ヒープ先頭の CPU ハンドル（Initialize 時キャッシュ）
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{};  ///< ヒープ先頭の GPU ハンドル（shader visible 時のみ有効）

    uint32_t descriptorSize_ = 0;      ///< ディスクリプタ1個分のサイズ（バイト単位）
    uint32_t numDescriptors_ = 0;      ///< ヒープの容量
    bool     shaderVisible_  = false;

    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> freeIndices_;  ///< 解放されたインデックスを小さい順に再利用する優先度付きキュー

    std::unordered_set<uint32_t> usedIndices_;  ///< 現在使用中のインデックス（高速な存在チェック用）

    uint32_t nextNewIndex_   = 1;  ///< 次に払い出す新規インデックス（0 は無効番兵として予約）
    uint32_t allocatedCount_ = 0;  ///< 現在確保されているインデックスの総数
  };

} // namespace Tako
