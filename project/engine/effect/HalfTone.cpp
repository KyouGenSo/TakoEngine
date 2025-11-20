#include "HalfTone.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "WinApp.h"

#include <cassert>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

void HalfTone::Initialize(DX12Basic* dx12, const std::string shaderName)
{
  IPostEffect::Initialize(dx12, shaderName);
  CreateCBV();
}

void HalfTone::Apply(const uint32_t inputSrvIndex, const D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, const uint32_t depthSrvIndex, const Vector4 clearColor)
{
  depthSrvIndex; // 深度バッファはこのエフェクトでは使用しないため、引数として受け取るが無視する
  clearColor;    // ClearColorもこのエフェクトでは使用しないため、引数として受け取るが無視する

  // スクリーンサイズを更新
  UpdateScreenSize();

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
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource_->GetGPUVirtualAddress());

  // レンダーテクスチャAをシェーダーリソースとして設定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

  // フルスクリーン三角形描画
  m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

void HalfTone::DrawImgui()
{
#ifdef _DEBUG
  ImGui::DragFloat("Dot Size", &cBufferData_->dotSize, 1.0f, 1.0f, 50.0f);
  ImGui::DragFloat("Contrast", &cBufferData_->contrast, 0.01f, 0.1f, 3.0f);
  ImGui::DragFloat("Angle", &cBufferData_->angle, 0.01f, 0.0f, 6.28f);
  ImGui::DragFloat("Threshold", &cBufferData_->threshold, 0.01f, 0.0f, 1.0f);
  
  // ドットパターン選択
  const char* patterns[] = { "Circle", "Square", "Diamond" };
  ImGui::Combo("Dot Pattern", &cBufferData_->dotPattern, patterns, 3);
  
  // カラーモード選択
  const char* colorModes[] = { "Monochrome", "CMYK" };
  ImGui::Combo("Color Mode", &cBufferData_->colorMode, colorModes, 2);
  
  ImGui::Text("Screen Size: %.0f x %.0f", cBufferData_->screenSize.x, cBufferData_->screenSize.y);
#endif
}

bool HalfTone::SetGenericParam(const EffectParam& param)
{
  if (auto* halfToneParam = std::get_if<HalfToneParam>(&param)) {
    SetParam(*halfToneParam);
    return true;
  }
  return false;
}

void HalfTone::SetParam(const HalfToneParam& param)
{
  if (cBufferData_ == nullptr)
  {
    return; // cBufferData_が初期化されていない場合は何もしない
  }
  // パラメータの設定
  cBufferData_->dotSize = param.dotSize;
  cBufferData_->contrast = param.contrast;
  cBufferData_->angle = param.angle;
  cBufferData_->dotPattern = param.dotPattern;
  cBufferData_->colorMode = param.colorMode;
  cBufferData_->threshold = param.threshold;
  // screenSizeは自動更新されるため、ここでは設定しない
}

void HalfTone::UpdateScreenSize()
{
  if (cBufferData_ != nullptr) {
    cBufferData_->screenSize.x = static_cast<float>(WinApp::clientWidth);
    cBufferData_->screenSize.y = static_cast<float>(WinApp::clientHeight);
  }
}

void HalfTone::CreateRootSignature()
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
  assert(SUCCEEDED(hr));
}

void HalfTone::CreatePSO()
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

void HalfTone::CreateCBV()
{
  // HalfToneParamのリソース生成
  cBufferResource_ = m_dx12_->MakeBufferResource(sizeof(HalfToneParam));

  // データの設定
  cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

  // データの初期化
  cBufferData_->dotSize = 8.0f;       // デフォルトのドットサイズ
  cBufferData_->contrast = 1.0f;      // デフォルトのコントラスト
  cBufferData_->angle = 0.0f;         // デフォルトの角度
  cBufferData_->dotPattern = 0;       // デフォルトは円形
  cBufferData_->colorMode = 0;        // デフォルトはモノクロ
  cBufferData_->threshold = 0.0f;     // デフォルトの閾値
  cBufferData_->padding = 0.0f;       // パディング
  
  // スクリーンサイズをWinAppから取得
  UpdateScreenSize();
}