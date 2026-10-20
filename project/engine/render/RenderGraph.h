#pragma once
#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>

#include "GpuResource.h"
#include "Vector4.h"

namespace Tako {

  class DX12Basic;

  /// <summary>
  /// RTV と SRV を持つ描画先テクスチャ
  /// </summary>
  struct RenderTexture {
    GpuResource                 resource;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
    uint32_t                    rtvIndex    = 0;
    uint32_t                    srvIndex    = 0;
    uint32_t                    width       = 0;
    uint32_t                    height      = 0;
    DXGI_FORMAT                 format      = DXGI_FORMAT_UNKNOWN;

    /// <summary>
    /// リソース生成と RTV/SRV の確保・ビュー生成
    /// </summary>
    /// <param name="dx12">DirectX 12基盤システムのポインタ</param>
    /// <param name="textureWidth">テクスチャの幅（ピクセル）</param>
    /// <param name="textureHeight">テクスチャの高さ（ピクセル）</param>
    /// <param name="textureFormat">ピクセルフォーマット</param>
    /// <param name="clearColor">クリアカラー</param>
    /// <param name="forceOpaqueAlpha">SRV の α を常に 1 として読ませる (最終表示用 RT 向け)</param>
    void Create(DX12Basic* dx12, uint32_t textureWidth, uint32_t textureHeight, DXGI_FORMAT textureFormat, const Vector4& clearColor, bool forceOpaqueAlpha = false);

    /// <summary>
    /// リソース解放と RTV/SRV インデックスの返却
    /// </summary>
    void Release();
  };

  /// <summary>
  /// パスが使う一時 RT を貸し出し、パスが読み書きするリソースを宣言どおりの状態にしてから実行する
  /// ponytail: パスは AddPass の時点で実行する（記録してから並べ替える段は持たない）。パスの省略や RT のメモリ共有が要るなら記録→実行の 2 段に分ける
  /// </summary>
  class RenderGraph {
  public: //構造体
    struct TextureDesc {
      uint32_t    width        = 0;
      uint32_t    height       = 0;
      DXGI_FORMAT format       = DXGI_FORMAT_R8G8B8A8_UNORM;
      Vector4     clearColor{};                               ///< 最適化クリア値（これ以外の色でクリアするとデバッグレイヤーが警告する）

      bool operator==(const TextureDesc&) const = default;
    };

    /// <summary>
    /// パスが使うリソースと、そのとき必要な状態
    /// </summary>
    struct Access {
      GpuResource&          resource;
      D3D12_RESOURCE_STATES state;
    };

  public: //メンバー関数
    void Initialize(DX12Basic* dx12) { dx12_ = dx12; }

    /// <summary>
    /// 全ての RT を解放する
    /// </summary>
    void Finalize();

    /// <summary>
    /// 前のフレームで貸し出さなかった RT を解放し、残りを貸し出せる状態に戻す。GPU が前のフレームを描き終えてから呼ぶ
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// desc に合う RT を貸し出す。Release するまで（遅くとも次の BeginFrame まで）他には貸さない
    /// </summary>
    RenderTexture& Acquire(const TextureDesc& desc);

    void Release(const RenderTexture& texture);

    /// <summary>
    /// accesses のリソースを指定の状態にしてから execute を実行する
    /// </summary>
    void AddPass(std::initializer_list<Access> accesses, const std::function<void()>& execute);

    static Access Read(GpuResource& resource) { return { resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE }; }
    static Access Write(GpuResource& resource) { return { resource, D3D12_RESOURCE_STATE_RENDER_TARGET }; }
    static Access DepthWrite(GpuResource& resource) { return { resource, D3D12_RESOURCE_STATE_DEPTH_WRITE }; }

    /// <summary>
    /// texture と同じ大きさ・フォーマットの一時 RT の desc
    /// </summary>
    static TextureDesc DescOf(const RenderTexture& texture) { return { texture.width, texture.height, texture.format }; }

  private: //構造体
    struct Entry {
      TextureDesc                    desc;
      std::unique_ptr<RenderTexture> texture;          ///< 貸し出した参照を pool_ の伸長で無効にしないため個別に確保する
      bool                           inUse   = false;
      bool                           used    = false;  ///< BeginFrame 以降に貸し出したか
    };

  private: //メンバー変数
    DX12Basic*         dx12_ = nullptr;
    std::vector<Entry> pool_;
  };

} // namespace Tako
