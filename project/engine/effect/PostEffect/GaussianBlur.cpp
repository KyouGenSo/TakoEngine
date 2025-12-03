#include "GaussianBlur.h"
#include "Bloom.h"

#include "DX12Basic.h"
#include "WinApp.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

GaussianBlur::~GaussianBlur()
{
  if (winApp_ && onResizeId_ != 0)
  {
    winApp_->UnregisterOnResizeFunc(onResizeId_);
  }
}

void GaussianBlur::Initialize(DX12Basic* dx12, const std::string& shaderName)
{
  IPostEffect::Initialize(dx12, shaderName);
  CreateCBV();
  CreateRenderTexture();

  // WinAppのインスタンスを取得してリサイズコールバックを登録
  winApp_ = WinApp::GetInstance();
  if (winApp_)
  {
    onResizeId_ = winApp_->RegisterOnResizeFunc(
      std::bind(&GaussianBlur::OnResize, this, std::placeholders::_1)
    );
  }
}

void GaussianBlur::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, const Vector4& clearColor)
{
  depthSrvIndex; // 深度バッファはこのエフェクトでは使用しないため、引数として受け取るが無視する
  clearColor;    // ClearColorもこのエフェクトでは使用しないため、引数として受け取るが無視する


  //---------------------------Pass1---------------------------//
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();

  m_dx12_->GetCommandList()->OMSetRenderTargets(1,
    &resultRT_.rtvHandle,
    false,
    &dsvHandle);

  // エフェクト適用シェーダーの設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
  m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

  // プリミティブトポロジーの設定（フルスクリーン三角形用）
  m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  // パラメータリソースの設定
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource1_->GetGPUVirtualAddress());

  // レンダーテクスチャAをシェーダーリソースとして設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

  // 描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);


  //---------------------------Pass2---------------------------//
  SetBarrier(resultRT_.resource.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  // 最終結果レンダーテクスチャを描画先に設定
  m_dx12_->GetCommandList()->OMSetRenderTargets(1,
    &outputRtvHandle,
    false,
    &dsvHandle);

  // BloomParamをセット
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource2_->GetGPUVirtualAddress());

  // ブラー画像をシェーダーリソースとして設定（スロット0）
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, resultRT_.srvIndex);

  // 描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

  SetBarrier(resultRT_.resource.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void GaussianBlur::DrawImgui()
{
#ifdef _DEBUG
  ImGui::DragFloat("Blur Sigma", &cBufferData1_->sigma, 0.01f, 0.0f, 50.0f);
  cBufferData2_->sigma = cBufferData1_->sigma; // Pass2でも同じ値を使用
  ImGui::DragInt("Blur Kernel Size", reinterpret_cast<int*>(&cBufferData1_->kernelSize), 1.0f, 1, 100);
  cBufferData2_->kernelSize = cBufferData1_->kernelSize; // Pass2でも同じ値を使用
#endif
}

bool GaussianBlur::SetGenericParam(const EffectParam& param)
{
  if (auto* blurParam = std::get_if<GaussianBlurParam>(&param)) {
    SetParam(*blurParam);
    return true;
  }
  return false;
}

void GaussianBlur::SetParam(const GaussianBlurParam& param)
{
  if (cBufferData1_ == nullptr)
  {
    return; // cBufferData_が初期化されていない場合は何もしない
  }
  // パラメータを設定する
  cBufferData1_->sigma = param.sigma;
  cBufferData1_->kernelSize = param.kernelSize;

  if (cBufferData2_ == nullptr)
  {
    return;
  }
  cBufferData2_->sigma = param.sigma;
  cBufferData2_->kernelSize = param.kernelSize;
}

void GaussianBlur::CreateRootSignature()
{
  HRESULT hr;

  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // Samplerの設定
  D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
  samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
  samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
  samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
  samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大LOD
  samplerDesc[0].ShaderRegister = 0; // レジスタ番号
  samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  descriptionRootSignature.pStaticSamplers = samplerDesc;
  descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

  // DescriptorRangeの設定。
  D3D12_DESCRIPTOR_RANGE descriptorRangesForTex[1] = {};
  descriptorRangesForTex[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRangesForTex[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangesForTex[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangesForTex[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。複数設定できるので配列
  D3D12_ROOT_PARAMETER rootParameters[2] = {};

  // Texture
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangesForTex; // ディスクリプタレンジを設定
  rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangesForTex); // レンジの数

  // Param
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

  descriptionRootSignature.pParameters = rootParameters;
  descriptionRootSignature.NumParameters = _countof(rootParameters);

  Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

  hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
  if (FAILED(hr))
  {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), DebugUIManager::LogType::Error);
#endif
    assert(false);
  }

  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
  signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf());
  assert(SUCCEEDED(hr));
}

void GaussianBlur::CreatePSO()
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

  // shaderのコンパイル
  Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/FullScreen.VS.hlsl", L"vs_6_0");
  assert(vertexShaderBlob != nullptr);

  std::wstring psPath = L"resources/shaders/" + StringUtility::ConvertString(shaderName_) + L".PS.hlsl";
  Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(psPath, L"ps_6_0");
  assert(pixelShaderBlob != nullptr);

  // DepthStencilState
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
  // depthの機能を無効にする
  depthStencilDesc.DepthEnable = false;

  // PSOの生成
  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
  graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
  graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
  graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
  graphicsPipelineStateDesc.BlendState = blendDesc;
  graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
  // 書き込むRTVの情報
  graphicsPipelineStateDesc.NumRenderTargets = 1;
  graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  // 利用するトポロジ（形状）のタイプ。三角形
  graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // どのように画面に色を打ち込むかの設定
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  // DepthStencilの設定
  graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

  // 実際に生成
  hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
  assert(SUCCEEDED(hr));
}

void GaussianBlur::CreateCBV()
{
  // BloomParamのリソース生成
  cBufferResource1_ = m_dx12_->MakeBufferResource(sizeof(GaussianBlurParam));
  cBufferResource2_ = m_dx12_->MakeBufferResource(sizeof(GaussianBlurParam));

  // データの設定
  cBufferResource1_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData1_));
  cBufferResource2_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData2_));

  // データの初期化
  cBufferData1_->sigma = 2.0f;
  cBufferData1_->direction = { 0.0f, 1.0f };
  cBufferData1_->kernelSize = 9;

  cBufferData2_->sigma = 2.0f;
  cBufferData2_->direction = { 1.0f, 0.0f };
  cBufferData2_->kernelSize = 9;
}

void GaussianBlur::CreateRenderTexture()
{
  auto createRT = [this](RenderTexture& rt, int rtvIndex, const Vector4& clearColor) {
    // リソース作成
    m_dx12_->CreateRenderTextureResource(
      rt.resource,
      WinApp::clientWidth,
      WinApp::clientHeight,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      clearColor
    );

    // RTV作成
    rt.rtvHandle = m_dx12_->GetRenderTextureRTVHandle(rtvIndex);
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    m_dx12_->GetDevice()->CreateRenderTargetView(
      rt.resource.Get(), &rtvDesc, rt.rtvHandle
    );

    // SRV作成
    rt.srvIndex = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForTexture2D(
      rt.srvIndex, rt.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1
    );
    };

  createRT(resultRT_, 9, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
}

void GaussianBlur::OnResize(const Vector2& newSize)
{
  newSize; // 未使用の警告を抑制
  
  // RenderTextureを再作成
  RecreateRenderTexture();
}

void GaussianBlur::RecreateRenderTexture()
{
  resultRT_.resource.Reset();
  SrvManager::GetInstance()->Free(resultRT_.srvIndex);

  CreateRenderTexture();
}

void GaussianBlur::SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource;
  barrier.Transition.StateBefore = stateBefore;
  barrier.Transition.StateAfter = stateAfter;

  m_dx12_->GetCommandList()->ResourceBarrier(1, &barrier);
}
