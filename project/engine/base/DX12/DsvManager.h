#pragma once
#include <d3d12.h>
#include <memory>
#include "DescriptorHeap.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// DSV ディスクリプタ管理クラス
  /// フリーリスト方式によるインデックス管理
  /// </summary>
  class DsvManager {
  private: // シングルトン設定
    static std::unique_ptr<DsvManager> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~DsvManager() = default;

    friend struct std::default_delete<DsvManager>;

  public:
    explicit DsvManager(Token) {}
    DsvManager(const DsvManager&) = delete;
    DsvManager& operator=(const DsvManager&) = delete;

  public: //定数
    static constexpr uint32_t kMaxDSVCount = 8;  ///< 最大 DSV 数
    static constexpr uint32_t kInvalidIndex = DescriptorHeap::kInvalidIndex;  ///< 無効番兵。Allocate 失敗時もこの値を返す

  public: //メンバー関数

    /// <summary>
    /// インスタンスの取得
    /// </summary>
    /// <returns>DsvManager のシングルトンインスタンス</returns>
    static DsvManager* GetInstance();

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
    /// DSV の確保
    /// </summary>
    /// <returns>確保された DSV のインデックス。枯渇時は kInvalidIndex</returns>
    uint32_t Allocate();

    /// <summary>
    /// DSV の解放
    /// </summary>
    /// <param name="dsvIndex">解放する DSV のインデックス</param>
    void Free(uint32_t dsvIndex);

    /// <summary>
    /// DSV 生成（2D テクスチャ用）
    /// </summary>
    /// <param name="dsvIndex">DSV を作成するインデックス</param>
    /// <param name="pResource">深度ステンシルリソース</param>
    /// <param name="format">深度フォーマット</param>
    void CreateDSV(uint32_t dsvIndex, ID3D12Resource* pResource, DXGI_FORMAT format);

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// 使用中かどうかチェック
    /// </summary>
    /// <param name="dsvIndex">チェックする DSV のインデックス</param>
    /// <returns>使用中の場合 true、未使用の場合 false</returns>
    bool IsAllocated(uint32_t dsvIndex) const;

    /// <summary>
    /// 指定番号の CPU ディスクリプタハンドルを取得
    /// </summary>
    /// <param name="dsvIndex">取得するディスクリプタのインデックス</param>
    /// <returns>CPU ディスクリプタハンドル</returns>
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t dsvIndex) const;

    uint32_t GetAllocatedCount() const { return heap_.GetAllocatedCount(); }

  private: //メンバー変数

    DX12Basic* dx12_ = nullptr;  ///< DirectX 12基盤システムへのポインタ

    DescriptorHeap heap_;  ///< DSV 用ディスクリプタヒープ＋インデックスアロケータ

  };

} // namespace Tako
