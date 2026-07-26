#include "Bloom.h"

#include "DX12Basic.h"
#include "WinApp.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

  Bloom::~Bloom()
  {
    if (winApp_ && onResizeId_ != 0) {
      winApp_->UnregisterOnResizeFunc(onResizeId_);
    }

    // RT の RTV/SRV を返却
    highLumRT_.Release();
    blurRT_.Release();
    resultRT_.Release();
  }

  void Bloom::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreatePSO("ThresholdExtract");
    CreatePSO("GaussianBlur");
    CreatePSO("BloomCombine");
    CreateCBV();
    CreateRenderTexture();

    // WinApp のインスタンスを取得してリサイズコールバックを登録
    winApp_ = WinApp::GetInstance();
    if (winApp_) {
      onResizeId_ = winApp_->RegisterOnResizeFunc(
        std::bind(&Bloom::OnResize, this, std::placeholders::_1)
      );
    }
  }

  void Bloom::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    //---------------------------Pass1 HighLunExtract---------------------------//
    DrawFullScreenPass(rootSignatures_["ThresholdExtract"].Get(), pipelineStates_["ThresholdExtract"].Get(),
      highLumRT_.rtvHandle, extractCBufferRes_->GetGPUVirtualAddress(), inputSrvIndex);

    //---------------------------Pass2 HorizontalBlur---------------------------//
    SetBarrier(highLumRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    DrawFullScreenPass(rootSignatures_["GaussianBlur"].Get(), pipelineStates_["GaussianBlur"].Get(),
      blurRT_.rtvHandle, blurCBufferRes1_->GetGPUVirtualAddress(), highLumRT_.srvIndex);

    //---------------------------Pass3 VerticalBlur---------------------------//
    SetBarrier(blurRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    DrawFullScreenPass(rootSignatures_["GaussianBlur"].Get(), pipelineStates_["GaussianBlur"].Get(),
      resultRT_.rtvHandle, blurCBufferRes2_->GetGPUVirtualAddress(), blurRT_.srvIndex);

    //---------------------------Pass4 Combine---------------------------//
    SetBarrier(resultRT_.resource.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    dx12_->GetCommandList()->OMSetRenderTargets(1,
      &outputRtvHandle,
      false,
      nullptr);

    dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignatures_["BloomCombine"].Get());
    dx12_->GetCommandList()->SetPipelineState(pipelineStates_["BloomCombine"].Get());

    dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, combineCBufferRes_->GetGPUVirtualAddress());

    // t0=元画像, t1(スロット2)=ブラー結果
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, resultRT_.srvIndex);

    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);


    //---------------------------リソースステートを戻す---------------------------//
    SetBarrier(highLumRT_.resource.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_RENDER_TARGET);

    SetBarrier(blurRT_.resource.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_RENDER_TARGET);

    SetBarrier(resultRT_.resource.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_RENDER_TARGET);
  }

  void Bloom::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("Bloom Intensity", &combineData_->intensity, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Bloom Threshold", &extractData_->threshold, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Bloom Sigma", &blurData1_->sigma, 0.01f, 0.0f, 50.0f);
    blurData2_->sigma = blurData1_->sigma; // Pass2でも同じ値を使用
    ImGui::DragInt("Bloom Kernel Size", reinterpret_cast<int*>(&blurData1_->kernelSize), 1.0f, 1, 100);
    blurData2_->kernelSize = blurData1_->kernelSize; // Pass2でも同じ値を使用
#endif
  }

  bool Bloom::SetGenericParam(const EffectParam& param)
  {
    if (auto* bloomParam = std::get_if<HighLumExtrcatParam>(&param)) {
      SetParam(*bloomParam);
      return true;
    }

    if (auto* bloomParam = std::get_if<GaussianBlurParam>(&param)) {
      SetParam(*bloomParam);
      return true;
    }

    if (auto* bloomParam = std::get_if<BloomCombineParam>(&param)) {
      SetParam(*bloomParam);
      return true;
    }
    return false;
  }

  void Bloom::SetParam(const HighLumExtrcatParam& param)
  {
    if (extractData_ == nullptr) {
      return;
    }
    extractData_->threshold = param.threshold;
  }

  void Bloom::SetParam(const GaussianBlurParam& param)
  {
    if (blurData1_ == nullptr) {
      return;
    }
    blurData1_->sigma = param.sigma;
    blurData1_->kernelSize = param.kernelSize;

    if (blurData2_ == nullptr) {
      return;
    }
    blurData2_->sigma = param.sigma;
    blurData2_->kernelSize = param.kernelSize;
  }

  void Bloom::SetParam(const BloomCombineParam& param)
  {
    if (combineData_ == nullptr) {
      return;
    }
    combineData_->intensity = param.intensity;
  }

  void Bloom::CreateRootSignature()
  {
    // Bloom はパス毎に rootSignatures_/pipelineStates_ を構築するため、基底の単一 rootSignature_ は未使用
  }

  void Bloom::CreateRootSignature(const std::string& shaderName)
  {
    shaderName;
    HRESULT hr;

    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Sampler の設定
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc[0].ShaderRegister = 0;
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

    D3D12_DESCRIPTOR_RANGE descriptorRangesForTex1[1] = {};
    descriptorRangesForTex1[0].BaseShaderRegister = 0;
    descriptorRangesForTex1[0].NumDescriptors = 1;
    descriptorRangesForTex1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangesForTex1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descriptorRangesForTex2[1] = {};
    descriptorRangesForTex2[0].BaseShaderRegister = 1;
    descriptorRangesForTex2[0].NumDescriptors = 1;
    descriptorRangesForTex2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangesForTex2[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    // Texture1
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangesForTex1;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangesForTex1);

    // Param
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // Texture2
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangesForTex2;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangesForTex2);

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
      assert(false);
    }

    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignatures_[shaderName].GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void Bloom::CreatePSO()
  {
    // Bloom はパス毎に pipelineStates_ を構築するため、基底の単一 pipelineState_ は未使用
  }

  void Bloom::CreatePSO(const std::string& shaderName)
  {
    CreateRootSignature(shaderName);

    HRESULT hr;

    // InputLayout
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = nullptr;
    inputLayoutDesc.NumElements = 0;

    // BlendState
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasterizerState
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

    // shader のコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"FullScreen.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    std::wstring psPath = EnginePaths::ShaderPath(StringUtility::ConvertString(shaderName) + L".PS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx12_->CompileShader(psPath, L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // DepthStencilState
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignatures_[shaderName].Get();
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
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineStates_[shaderName]));
    assert(SUCCEEDED(hr));
  }

  void Bloom::CreateCBV()
  {
    extractCBufferRes_ = dx12_->MakeBufferResource(sizeof(HighLumExtrcatParam));
    blurCBufferRes1_ = dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
    blurCBufferRes2_ = dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
    combineCBufferRes_ = dx12_->MakeBufferResource(sizeof(BloomCombineParam));

    // データの設定
    extractCBufferRes_->Map(0, nullptr, reinterpret_cast<void**>(&extractData_));
    blurCBufferRes1_->Map(0, nullptr, reinterpret_cast<void**>(&blurData1_));
    blurCBufferRes2_->Map(0, nullptr, reinterpret_cast<void**>(&blurData2_));
    combineCBufferRes_->Map(0, nullptr, reinterpret_cast<void**>(&combineData_));

    // データの初期化
    extractData_->threshold = 1.0f;

    blurData1_->sigma = 2.0f;
    blurData1_->kernelSize = 10;
    blurData1_->direction = Vector2(1.0f, 0.0f); // 水平方向

    blurData2_->sigma = 2.0f;
    blurData2_->kernelSize = 10;
    blurData2_->direction = Vector2(0.0f, 1.0f); // 垂直方向

    combineData_->intensity = 1.0f;
  }

  void Bloom::CreateRenderTexture()
  {
    const Vector4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    highLumRT_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
    blurRT_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
    resultRT_.Create(dx12_, WinApp::clientWidth, WinApp::clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
  }

  void Bloom::OnResize(const Vector2& newSize)
  {
    newSize; // 未使用の警告を抑制

    // RenderTexture を再作成
    RecreateRenderTexture();
  }

  void Bloom::RecreateRenderTexture()
  {
    resultRT_.Release();
    highLumRT_.Release();
    blurRT_.Release();

    CreateRenderTexture();
  }

  void Bloom::SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
  {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;

    dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
  }

} // namespace Tako
