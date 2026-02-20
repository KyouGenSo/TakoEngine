#include "Fog.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "Object3dBasic.h"
#include "Camera.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void Fog::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void Fog::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_DEPTH_WRITE,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      m_dx12_->GetDepthStencilResource());

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();
    m_dx12_->GetCommandList()->OMSetRenderTargets(1,
      &outputRtvHandle,
      false,
      &dsvHandle);

    // エフェクト適用シェーダーの設定
    m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジーの設定（フルスクリーン三角形用）
    m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // パラメータリソースの設定
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, fogParamResource_->GetGPUVirtualAddress());
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());

    // 深度テクスチャをシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, depthSrvIndex);

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

    m_dx12_->TransitionResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      m_dx12_->GetDepthStencilResource());
  }

  void Fog::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::ColorEdit4("FogColor", &fogData_->color.x);
    ImGui::DragFloat("Density", &fogData_->density, 0.001f, 0.0f, 1.0f);
#endif
  }

  bool Fog::SetGenericParam(const EffectParam& param)
  {
    if (auto* fogParam = std::get_if<FogParam>(&param)) {
      SetParam(*fogParam);
      return true;
    }
    return false;
  }

  void Fog::SetParam(const FogParam& param)
  {
    if (fogData_ == nullptr) {
      return; // cBufferData_が初期化されていない場合は何もしない
    }
    // パラメータを更新
    fogData_->color = param.color;
    fogData_->density = param.density;
  }

  void Fog::CreateRootSignature()
  {
    HRESULT hr;

    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Sampler の設定
    D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
    samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // テクスチャの補間方法
    samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
    samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
    samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
    samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
    samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大 LOD
    samplerDesc[0].ShaderRegister = 0; // レジスタ番号
    samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    descriptionRootSignature.pStaticSamplers = samplerDesc;
    descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

    // DescriptorRange の設定。
    D3D12_DESCRIPTOR_RANGE descriptorRangesForTex[1] = {};
    descriptorRangesForTex[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRangesForTex[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangesForTex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRangesForTex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForDSV[1] = {};
    descriptorRangeForDSV[0].BaseShaderRegister = 1; // レジスタ番号
    descriptorRangeForDSV[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForDSV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRangeForDSV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    // RootParameter の設定。複数設定できるので配列
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    // Texture
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangesForTex; // ディスクリプタレンジを設定
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangesForTex); // レンジの数

    // FogParam
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

    // DepthResource
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForDSV; // ディスクリプタレンジを設定
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForDSV); // レンジの数

    // CameraForGPU
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
    rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド

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

    hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf());
    assert(SUCCEEDED(hr));
  }

  void Fog::CreatePSO()
  {
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
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    // 裏面を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

    // shader のコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/FullScreen.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    std::wstring psPath = L"resources/shaders/" + StringUtility::ConvertString(shaderName_) + L".PS.hlsl";
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(psPath, L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // DepthStencilState
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    // depth の機能を無効にする
    depthStencilDesc.DepthEnable = false;

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    // 書き込む RTV の情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // 利用するトポロジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    // DepthStencil の設定
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 実際に生成
    hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
  }

  void Fog::CreateCBV()
  {
    fogParamResource_ = m_dx12_->MakeBufferResource(sizeof(FogParam));

    // map
    fogParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&fogData_));

    // 初期値を設定
    fogData_->color = Vector4(1.f, 1.f, 1.f, 1.0f);
    fogData_->density = 0.0f;


    // camera resource の生成--------------------------------------------------------------------------------
    cameraResource_ = m_dx12_->MakeBufferResource(sizeof(CameraForGPU));

    // map
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    // 初期値を設定
    cameraData_->farPlane = (*Object3dBasic::GetInstance()->GetCamera())->GetFarClip();
    cameraData_->nearPlane = (*Object3dBasic::GetInstance()->GetCamera())->GetNearClip();

    // unmap
    cameraResource_->Unmap(0, nullptr);
  }

} // namespace Tako