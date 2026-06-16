#include "IPostEffect.h"

#include "DX12Basic.h"
#include "StringUtility.h"
#include "EnginePaths.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

#include <cassert>
#include <vector>

namespace Tako {

  void IPostEffect::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    m_dx12_ = dx12;

    shaderName_ = shaderName;

    CreateRootSignature();
    CreatePSO();
  }

  void IPostEffect::BuildRootSignature(std::initializer_list<RootParam> params,
                                       SamplerFilterMode filter,
                                       SamplerAddressMode addrUV)
  {
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 静的サンプラー (1個)
    const D3D12_TEXTURE_ADDRESS_MODE address =
      (addrUV == SamplerAddressMode::Clamp) ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = (filter == SamplerFilterMode::Point) ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc[0].AddressU = address;
    samplerDesc[0].AddressV = address;
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[0].ShaderRegister = 0;
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

    // SRV テーブルごとに DescriptorRange を1個確保。reserve でアドレスを固定し、
    // ルートパラメータからの参照が再確保で無効化されないようにする
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
    ranges.reserve(params.size());
    std::vector<D3D12_ROOT_PARAMETER> rootParameters;
    rootParameters.reserve(params.size());

    for (const RootParam& p : params) {
      D3D12_ROOT_PARAMETER rp{};
      rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
      if (p.kind == RootParam::SrvTable) {
        D3D12_DESCRIPTOR_RANGE range{};
        range.BaseShaderRegister = p.shaderRegister;
        range.NumDescriptors = 1;
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        ranges.push_back(range);
        rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp.DescriptorTable.pDescriptorRanges = &ranges.back();
        rp.DescriptorTable.NumDescriptorRanges = 1;
      } else {
        rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rp.Descriptor.ShaderRegister = p.shaderRegister;
      }
      rootParameters.push_back(rp);
    }
    descriptionRootSignature.pParameters = rootParameters.data();
    descriptionRootSignature.NumParameters = static_cast<UINT>(rootParameters.size());

    ComPtr<ID3DBlob> signatureBlob = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }

    hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void IPostEffect::BuildFullScreenPSO()
  {
    // InputLayout (フルスクリーン三角形は VS 側で生成するため不要)
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = nullptr;
    inputLayoutDesc.NumElements = 0;

    // BlendState (不透明・書き込みのみ)
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasterizerState
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

    // shader のコンパイル
    ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(EnginePaths::ShaderPath(L"FullScreen.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    std::wstring psPath = EnginePaths::ShaderPath(StringUtility::ConvertString(shaderName_) + L".PS.hlsl");
    ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(psPath, L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // DepthStencilState (深度無効)
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    HRESULT hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
  }

} // namespace Tako
