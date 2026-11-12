#pragma once
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <wrl.h>

#include "RenderState.h"
#include "ShaderBindingLayout.h"

namespace Tako {

  class Shader;

  /// <summary>
  /// グラフィックスパイプラインの描画先に依存しない部分。描画先のフォーマットは描画時に CommandContext が補う
  /// </summary>
  struct GraphicsPipelineDesc {
    const Shader*          vs           = nullptr;
    const Shader*          ps           = nullptr;  ///< nullptr なら深度だけ書く
    VertexLayout           vertexLayout = VertexLayout::None;
    RenderState            renderState{};
    D3D_PRIMITIVE_TOPOLOGY topology     = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    bool operator==(const GraphicsPipelineDesc&) const = default;
  };

  struct ComputePipelineDesc {
    const Shader* cs = nullptr;

    bool operator==(const ComputePipelineDesc&) const = default;
  };

  /// <summary>
  /// 描画先のフォーマット（UNKNOWN はそのターゲットなし）
  /// </summary>
  struct RenderTargetFormats {
    DXGI_FORMAT color = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depth = DXGI_FORMAT_UNKNOWN;

    bool operator==(const RenderTargetFormats&) const = default;
  };

  /// <summary>
  /// シェーダーの組ごとのルートシグネチャと、描画設定 × 描画先ごとの PSO を必要になった時点で作って保持する
  /// </summary>
  class PipelineCache {
  private: //構造体
    struct LayoutEntry {
      std::unique_ptr<ShaderBindingLayout> layout;
      uint32_t                             versions[2] = {};
    };

    struct GraphicsKey {
      GraphicsPipelineDesc desc;
      RenderTargetFormats  formats;

      bool operator==(const GraphicsKey&) const = default;
    };

    struct GraphicsKeyHash {
      size_t operator()(const GraphicsKey& key) const;
    };

    struct PipelineEntry {
      Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
      uint32_t                                    versions[2] = {};
    };

    struct ShaderPairHash {
      size_t operator()(const std::pair<const Shader*, const Shader*>& pair) const;
    };

  public: //メンバー関数
    void Initialize(ID3D12Device* device) { device_ = device; }

    /// <summary>
    /// VS/PS の組のレイアウトを返す。シェーダーが再コンパイルされていれば作り直す
    /// </summary>
    const ShaderBindingLayout* GetGraphicsLayout(const Shader* vs, const Shader* ps);

    /// <summary>
    /// CS のレイアウトを返す。シェーダーが再コンパイルされていれば作り直す
    /// </summary>
    const ShaderBindingLayout* GetComputeLayout(const Shader* cs);

    ID3D12PipelineState* GetGraphicsPipeline(const GraphicsPipelineDesc& desc, const RenderTargetFormats& formats);

    ID3D12PipelineState* GetComputePipeline(const ComputePipelineDesc& desc);

  private: //非公開関数
    const ShaderBindingLayout* GetLayout(const Shader* first, const Shader* second, bool usesInputAssembler);

    /// <summary>
    /// 内容が同じルートシグネチャは 1 つを共有する
    /// </summary>
    ID3D12RootSignature* GetRootSignature(const ShaderBindingLayout& layout);

  private: //メンバー変数
    ID3D12Device* device_ = nullptr;

    std::unordered_map<std::pair<const Shader*, const Shader*>, LayoutEntry, ShaderPairHash>    layouts_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>>                rootSignatures_;  ///< シリアライズ結果のバイト列で引く
    std::unordered_map<GraphicsKey, PipelineEntry, GraphicsKeyHash>                             graphicsPipelines_;
    std::unordered_map<const Shader*, PipelineEntry>                                            computePipelines_;
  };

} // namespace Tako
