#include "PipelineCache.h"

#include <cassert>
#include <functional>
#include <span>

#include "ShaderLibrary.h"

namespace Tako {

  namespace {

    D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode)
    {
      D3D12_BLEND_DESC desc{};
      D3D12_RENDER_TARGET_BLEND_DESC& target = desc.RenderTarget[0];
      target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
      if (mode == BlendMode::Opaque) {
        return desc;
      }

      target.BlendEnable = true;
      target.BlendOp = D3D12_BLEND_OP_ADD;
      target.SrcBlendAlpha = D3D12_BLEND_ONE;
      target.DestBlendAlpha = D3D12_BLEND_ZERO;
      target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
      switch (mode) {
      case BlendMode::Add:
        target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        target.DestBlend = D3D12_BLEND_ONE;
        break;
      case BlendMode::Screen:
        target.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
        target.DestBlend = D3D12_BLEND_ONE;
        break;
      case BlendMode::Alpha:
        target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        break;
      case BlendMode::Multiply:
        target.SrcBlend = D3D12_BLEND_ZERO;
        target.DestBlend = D3D12_BLEND_SRC_COLOR;
        break;
      case BlendMode::Premultiplied:
        target.SrcBlend = D3D12_BLEND_ONE;
        target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        break;
      default:
        assert(false && "PipelineCache: unknown blend mode");
        break;
      }
      return desc;
    }

    D3D12_RASTERIZER_DESC MakeRasterizerDesc(const RenderState& state)
    {
      // 深度クリップは従来の各 PSO と同じく無効のまま（ゼロ初期化の値）
      D3D12_RASTERIZER_DESC desc{};
      desc.FillMode = D3D12_FILL_MODE_SOLID;
      switch (state.cull) {
      case CullMode::None:  desc.CullMode = D3D12_CULL_MODE_NONE;  break;
      case CullMode::Front: desc.CullMode = D3D12_CULL_MODE_FRONT; break;
      case CullMode::Back:  desc.CullMode = D3D12_CULL_MODE_BACK;  break;
      }
      return desc;
    }

    D3D12_DEPTH_STENCIL_DESC MakeDepthStencilDesc(const RenderState& state, bool hasDepthTarget)
    {
      D3D12_DEPTH_STENCIL_DESC desc{};
      desc.DepthEnable = hasDepthTarget && state.depthTest;
      desc.DepthWriteMask = (desc.DepthEnable && state.depthWrite) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
      desc.DepthFunc = state.depthFunc;
      return desc;
    }

    std::span<const D3D12_INPUT_ELEMENT_DESC> InputElements(VertexLayout layout)
    {
      static const D3D12_INPUT_ELEMENT_DESC kMesh[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };
      static const D3D12_INPUT_ELEMENT_DESC kPosition[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };
      static const D3D12_INPUT_ELEMENT_DESC kSprite[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };
      static const D3D12_INPUT_ELEMENT_DESC kLine[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };
      static const D3D12_INPUT_ELEMENT_DESC kParticleInstance[] = {
        { "TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
      };

      switch (layout) {
      case VertexLayout::Mesh:             return kMesh;
      case VertexLayout::Position:         return kPosition;
      case VertexLayout::Sprite:           return kSprite;
      case VertexLayout::Line:             return kLine;
      case VertexLayout::ParticleInstance: return kParticleInstance;
      default:                             return {};
      }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType(D3D_PRIMITIVE_TOPOLOGY topology)
    {
      switch (topology) {
      case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
      case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
      default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      }
    }

    void HashCombine(size_t& seed, size_t value)
    {
      seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }

    uint32_t VersionOf(const Shader* shader) { return shader ? shader->GetVersion() : 0; }

  } // namespace

  size_t PipelineCache::GraphicsKeyHash::operator()(const GraphicsKey& key) const
  {
    const GraphicsPipelineDesc& desc = key.desc;
    const RenderState& state = desc.renderState;
    size_t seed = std::hash<const void*>{}(desc.vs);
    HashCombine(seed, std::hash<const void*>{}(desc.ps));
    HashCombine(seed, static_cast<size_t>(desc.vertexLayout));
    HashCombine(seed, static_cast<size_t>(desc.topology));
    HashCombine(seed, static_cast<size_t>(state.blend));
    HashCombine(seed, static_cast<size_t>(state.cull));
    HashCombine(seed, (state.depthTest ? 1u : 0u) | (state.depthWrite ? 2u : 0u));
    HashCombine(seed, static_cast<size_t>(state.depthFunc));
    HashCombine(seed, static_cast<size_t>(key.formats.color));
    HashCombine(seed, static_cast<size_t>(key.formats.depth));
    return seed;
  }

  size_t PipelineCache::ShaderPairHash::operator()(const std::pair<const Shader*, const Shader*>& pair) const
  {
    size_t seed = std::hash<const void*>{}(pair.first);
    HashCombine(seed, std::hash<const void*>{}(pair.second));
    return seed;
  }

  const ShaderBindingLayout* PipelineCache::GetGraphicsLayout(const Shader* vs, const Shader* ps)
  {
    return GetLayout(vs, ps, true);
  }

  const ShaderBindingLayout* PipelineCache::GetComputeLayout(const Shader* cs)
  {
    return GetLayout(cs, nullptr, false);
  }

  ID3D12PipelineState* PipelineCache::GetGraphicsPipeline(const GraphicsPipelineDesc& desc, const RenderTargetFormats& formats)
  {
    PipelineEntry& entry = graphicsPipelines_[GraphicsKey{ desc, formats }];
    if (entry.pso && entry.versions[0] == VersionOf(desc.vs) && entry.versions[1] == VersionOf(desc.ps)) {
      return entry.pso.Get();
    }

    const ShaderBindingLayout* layout = GetGraphicsLayout(desc.vs, desc.ps);
    const std::span<const D3D12_INPUT_ELEMENT_DESC> inputElements = InputElements(desc.vertexLayout);
    const bool hasColorTarget = formats.color != DXGI_FORMAT_UNKNOWN;
    const bool hasDepthTarget = formats.depth != DXGI_FORMAT_UNKNOWN;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = layout->GetRootSignature();
    psoDesc.VS = desc.vs->GetBytecode();
    if (desc.ps) {
      psoDesc.PS = desc.ps->GetBytecode();
    }
    psoDesc.BlendState = MakeBlendDesc(desc.renderState.blend);
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.RasterizerState = MakeRasterizerDesc(desc.renderState);
    psoDesc.DepthStencilState = MakeDepthStencilDesc(desc.renderState, hasDepthTarget);
    psoDesc.InputLayout = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
    psoDesc.PrimitiveTopologyType = TopologyType(desc.topology);
    psoDesc.NumRenderTargets = hasColorTarget ? 1 : 0;
    psoDesc.RTVFormats[0] = formats.color;
    psoDesc.DSVFormat = formats.depth;
    psoDesc.SampleDesc.Count = 1;

    entry.pso.Reset();
    [[maybe_unused]] const HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&entry.pso));
    assert(SUCCEEDED(hr) && "PipelineCache: CreateGraphicsPipelineState failed");
    entry.versions[0] = VersionOf(desc.vs);
    entry.versions[1] = VersionOf(desc.ps);
    return entry.pso.Get();
  }

  ID3D12PipelineState* PipelineCache::GetComputePipeline(const ComputePipelineDesc& desc)
  {
    PipelineEntry& entry = computePipelines_[desc.cs];
    if (entry.pso && entry.versions[0] == VersionOf(desc.cs)) {
      return entry.pso.Get();
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = GetComputeLayout(desc.cs)->GetRootSignature();
    psoDesc.CS = desc.cs->GetBytecode();

    entry.pso.Reset();
    [[maybe_unused]] const HRESULT hr = device_->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&entry.pso));
    assert(SUCCEEDED(hr) && "PipelineCache: CreateComputePipelineState failed");
    entry.versions[0] = VersionOf(desc.cs);
    return entry.pso.Get();
  }

  const ShaderBindingLayout* PipelineCache::GetLayout(const Shader* first, const Shader* second, bool usesInputAssembler)
  {
    LayoutEntry& entry = layouts_[{ first, second }];
    if (entry.layout && entry.versions[0] == VersionOf(first) && entry.versions[1] == VersionOf(second)) {
      return entry.layout.get();
    }

    const Shader* shaders[] = { first, second };
    entry.layout = std::make_unique<ShaderBindingLayout>(shaders, usesInputAssembler);
    entry.layout->SetRootSignature(GetRootSignature(*entry.layout));
    entry.versions[0] = VersionOf(first);
    entry.versions[1] = VersionOf(second);
    return entry.layout.get();
  }

  ID3D12RootSignature* PipelineCache::GetRootSignature(const ShaderBindingLayout& layout)
  {
    Microsoft::WRL::ComPtr<ID3DBlob> blob = layout.Serialize();
    std::string bytes(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());

    Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature = rootSignatures_[bytes];
    if (!rootSignature) {
      [[maybe_unused]] const HRESULT hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
      assert(SUCCEEDED(hr) && "PipelineCache: CreateRootSignature failed");
    }
    return rootSignature.Get();
  }

} // namespace Tako
