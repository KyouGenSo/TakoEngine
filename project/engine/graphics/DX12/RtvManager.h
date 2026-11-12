#pragma once
#include <d3d12.h>
#include <memory>
#include "DescriptorHeap.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// RTV ディスクリプタ管理クラス
  /// フリーリスト方式によるインデックス管理
  /// </summary>
  class RtvManager {
  private: // シングルトン設定
    static std::unique_ptr<RtvManager> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~RtvManager() = default;

    friend struct std::default_delete<RtvManager>;

  public:
    explicit RtvManager(Token) {}
    RtvManager(const RtvManager&) = delete;
    RtvManager& operator=(const RtvManager&) = delete;

  public: //定数
    static constexpr uint32_t kMaxRTVCount = 32;  ///< 最大 RTV 数
    static constexpr uint32_t kInvalidIndex = DescriptorHeap::kInvalidIndex;  ///< 無効番兵。Allocate 失敗時もこの値を返す

  public: //メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>RtvManager のシングルトンインスタンス</returns>
    static RtvManager* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dx12">DirectX 12基盤システムのポインタ</param>
    void Initialize(DX12Basic* dx12);

    /// <summary>
    /// 終了処理。ヒープのみ解放しインスタンスは温存する
    /// （以後の GetInstance() は有効なまま、Free() は no-op になる）
    /// </summary>
    void Finalize();

    /// <summary>
    /// RTV の確保
    /// </summary>
    /// <returns>確保された RTV のインデックス。枯渇時は kInvalidIndex</returns>
    uint32_t Allocate();

    /// <summary>
    /// RTV の解放
    /// </summary>
    /// <param name="rtvIndex">解放する RTV のインデックス</param>
    void Free(uint32_t rtvIndex);

    /// <summary>
    /// RTV 生成（2D テクスチャ用）
    /// </summary>
    /// <param name="rtvIndex">RTV を作成するインデックス</param>
    /// <param name="pResource">レンダーターゲットリソース</param>
    /// <param name="format">ピクセルフォーマット</param>
    void CreateRTV(uint32_t rtvIndex, ID3D12Resource* pResource, DXGI_FORMAT format);

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// 使用中かどうかチェック
    /// </summary>
    /// <param name="rtvIndex">チェックする RTV のインデックス</param>
    /// <returns>使用中の場合 true、未使用の場合 false</returns>
    bool IsAllocated(uint32_t rtvIndex) const;

    /// <summary>
    /// 指定番号の CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="rtvIndex">取得するディスクリプタのインデックス</param>
    /// <returns>CPU ディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t rtvIndex) const;

    uint32_t GetAllocatedCount() const { return heap_.GetAllocatedCount(); }

  private: //メンバー変数

    DX12Basic* dx12_ = nullptr;  ///< DirectX 12基盤システムへのポインタ

    DescriptorHeap heap_;  ///< RTV 用ディスクリプタヒープ＋インデックスアロケータ

  };

} // namespace Tako
