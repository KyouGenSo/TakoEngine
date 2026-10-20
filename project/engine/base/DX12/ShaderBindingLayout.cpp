#include "ShaderBindingLayout.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <tuple>

#include "ShaderLibrary.h"

namespace Tako {

  namespace {

    /// <summary>
    /// Common.hlsli の宣言と一致させる固定サンプラー（s0〜s4）
    /// </summary>
    constexpr uint32_t kStaticSamplerCount = 5;

    std::array<D3D12_STATIC_SAMPLER_DESC, kStaticSamplerCount> MakeStaticSamplers()
    {
      auto make = [](uint32_t shaderRegister, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address) {
        D3D12_STATIC_SAMPLER_DESC desc{};
        desc.Filter = filter;
        desc.AddressU = address;
        desc.AddressV = address;
        desc.AddressW = address;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        desc.MaxLOD = D3D12_FLOAT32_MAX;
        desc.ShaderRegister = shaderRegister;
        desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        return desc;
      };

      std::array<D3D12_STATIC_SAMPLER_DESC, kStaticSamplerCount> samplers = {
        make(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),   // gLinearWrap
        make(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),  // gLinearClamp
        make(2, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP),    // gPointWrap
        make(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),   // gPointClamp
        make(4, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER),  // gShadowCmp
      };
      samplers[4].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
      samplers[4].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
      return samplers;
    }

    D3D12_SHADER_VISIBILITY VisibilityOf(ShaderStage stage)
    {
      switch (stage) {
      case ShaderStage::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
      case ShaderStage::Pixel:  return D3D12_SHADER_VISIBILITY_PIXEL;
      default:                  return D3D12_SHADER_VISIBILITY_ALL;
      }
    }

    /// <summary>
    /// リソースの種類からルートパラメータの種類を決める。サンプラーなど対象外は false
    /// </summary>
    bool KindOf(D3D_SHADER_INPUT_TYPE type, ShaderParameter::Kind& kind)
    {
      switch (type) {
      case D3D_SIT_CBUFFER:
        kind = ShaderParameter::Kind::Cbv;
        return true;
      case D3D_SIT_TBUFFER:
      case D3D_SIT_TEXTURE:
      case D3D_SIT_STRUCTURED:
      case D3D_SIT_BYTEADDRESS:
        kind = ShaderParameter::Kind::Srv;
        return true;
      case D3D_SIT_UAV_RWTYPED:
      case D3D_SIT_UAV_RWSTRUCTURED:
      case D3D_SIT_UAV_RWBYTEADDRESS:
      case D3D_SIT_UAV_APPEND_STRUCTURED:
      case D3D_SIT_UAV_CONSUME_STRUCTURED:
      case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        kind = ShaderParameter::Kind::Uav;
        return true;
      default:
        return false;
      }
    }

  } // namespace

  ShaderBindingLayout::ShaderBindingLayout(std::span<const Shader* const> shaders, bool usesInputAssembler)
    : usesInputAssembler_(usesInputAssembler)
  {
    for (const Shader* shader : shaders) {
      if (!shader) {
        continue;
      }
      const D3D12_SHADER_VISIBILITY stageVisibility = VisibilityOf(shader->GetKey().stage);
      ID3D12ShaderReflection* reflection = shader->GetReflection();
      D3D12_SHADER_DESC shaderDesc{};
      reflection->GetDesc(&shaderDesc);

      for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
        D3D12_SHADER_INPUT_BIND_DESC bind{};
        reflection->GetResourceBindingDesc(i, &bind);

        if (bind.Type == D3D_SIT_SAMPLER) {
          assert(bind.Space == 0 && bind.BindPoint < kStaticSamplerCount && "ShaderBindingLayout: sampler must be declared in Common.hlsli");
          continue;
        }
        ShaderParameter::Kind kind{};
        if (!KindOf(bind.Type, kind)) {
          assert(false && "ShaderBindingLayout: unsupported resource type");
          continue;
        }

        // 別ステージが同名・同レジスタで使っていれば全ステージから見えるパラメータにまとめる
        auto same = std::find_if(parameters_.begin(), parameters_.end(), [&](const ShaderParameter& p) {
          return p.kind == kind && p.shaderRegister == bind.BindPoint && p.registerSpace == bind.Space && p.name == bind.Name;
        });
        if (same != parameters_.end()) {
          if (same->visibility != stageVisibility) {
            same->visibility = D3D12_SHADER_VISIBILITY_ALL;
          }
          continue;
        }

        ShaderParameter parameter;
        parameter.name = bind.Name;
        parameter.id = MakeShaderParamId(parameter.name);
        parameter.kind = kind;
        parameter.shaderRegister = bind.BindPoint;
        parameter.registerSpace = bind.Space;
        parameter.descriptorCount = (std::max)(1u, bind.BindCount);
        parameter.visibility = stageVisibility;
        parameters_.push_back(std::move(parameter));
      }
    }

    // 同じ内容のシェーダー群なら同じ並びになるよう、名前以外の属性で並べる
    std::sort(parameters_.begin(), parameters_.end(), [](const ShaderParameter& a, const ShaderParameter& b) {
      return std::tie(a.kind, a.registerSpace, a.shaderRegister, a.visibility) < std::tie(b.kind, b.registerSpace, b.shaderRegister, b.visibility);
    });
    assert(parameters_.size() <= kMaxParameters);
  }

  Microsoft::WRL::ComPtr<ID3DBlob> ShaderBindingLayout::Serialize() const
  {
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges(parameters_.size());
    std::vector<D3D12_ROOT_PARAMETER> rootParameters(parameters_.size());

    for (size_t i = 0; i < parameters_.size(); ++i) {
      const ShaderParameter& parameter = parameters_[i];
      D3D12_ROOT_PARAMETER& root = rootParameters[i];
      root.ShaderVisibility = parameter.visibility;

      if (parameter.kind == ShaderParameter::Kind::Cbv) {
        root.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root.Descriptor.ShaderRegister = parameter.shaderRegister;
        root.Descriptor.RegisterSpace = parameter.registerSpace;
        continue;
      }

      D3D12_DESCRIPTOR_RANGE& range = ranges[i];
      range.RangeType = (parameter.kind == ShaderParameter::Kind::Srv) ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      range.NumDescriptors = parameter.descriptorCount;
      range.BaseShaderRegister = parameter.shaderRegister;
      range.RegisterSpace = parameter.registerSpace;
      range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

      root.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      root.DescriptorTable.NumDescriptorRanges = 1;
      root.DescriptorTable.pDescriptorRanges = &range;
    }

    const auto samplers = MakeStaticSamplers();

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = static_cast<UINT>(rootParameters.size());
    desc.pParameters = rootParameters.data();
    desc.NumStaticSamplers = static_cast<UINT>(samplers.size());
    desc.pStaticSamplers = samplers.data();
    desc.Flags = usesInputAssembler_ ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    const HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
      if (error) {
        OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
      }
      assert(false && "ShaderBindingLayout: root signature serialization failed");
      return nullptr;
    }
    return signature;
  }

} // namespace Tako
