#include "ModelBasic.h"
#include "DX12Basic.h"
#include "SrvManager.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

void ModelBasic::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

	directoryFolderName_ = "resources";

	ModelFolderName_ = "Model";

  CreateCSPSO();
}

void ModelBasic::SetSkinningCSSetting()
{
  m_dx12_->GetCommandList()->SetComputeRootSignature(csRootSignature_.Get());
  m_dx12_->GetCommandList()->SetPipelineState(csPipelineState_.Get());
}

void ModelBasic::CreateCSRootSignature()
{
  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

  descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // DescriptorRangeの設定。
  D3D12_DESCRIPTOR_RANGE descriptorRangeForPallete[1] = {};
  descriptorRangeForPallete[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRangeForPallete[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForPallete[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangeForPallete[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForVertexInput[1] = {};
  descriptorRangeForVertexInput[0].BaseShaderRegister = 1; // レジスタ番号
  descriptorRangeForVertexInput[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForVertexInput[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangeForVertexInput[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForInfluence[1] = {};
  descriptorRangeForInfluence[0].BaseShaderRegister = 2; // レジスタ番号
  descriptorRangeForInfluence[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForInfluence[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangeForInfluence[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_DESCRIPTOR_RANGE descriptorRangeForVertexOutput[1] = {};
  descriptorRangeForVertexOutput[0].BaseShaderRegister = 0; // レジスタ番号
  descriptorRangeForVertexOutput[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForVertexOutput[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAVを使う
  descriptorRangeForVertexOutput[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  D3D12_ROOT_PARAMETER rootParameters[5] = {};

  // Pallete
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeForPallete; // ディスクリプタレンジを設定
  rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForPallete); // レンジの数

  // VertexInput
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForVertexInput; // ディスクリプタレンジを設定
  rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForVertexInput); // レンジの数

  // Influence
  rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInfluence; // ディスクリプタレンジを設定
  rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInfluence); // レンジの数

  // VertexOutput
  rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeForVertexOutput; // ディスクリプタレンジを設定
  rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForVertexOutput); // レンジの数

  // SkinnigInfo
  rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
  rootParameters[4].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

  descriptionRootSignature.pParameters = rootParameters;
  descriptionRootSignature.NumParameters = _countof(rootParameters);

  Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

  HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
  if (FAILED(hr))
  {
#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog(
      reinterpret_cast<char*>(errorBlob->GetBufferPointer()),
      DebugUIManager::LogType::Error);
#endif

    assert(false);
  }

  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(csRootSignature_.GetAddressOf()));
  signatureBlob->GetBufferSize(), IID_PPV_ARGS(csRootSignature_.GetAddressOf());
  assert(SUCCEEDED(hr));

#ifdef _DEBUG
  DebugUIManager::GetInstance()->AddLog("CSRootSignature Created", DebugUIManager::LogType::Info);
#endif

}

void ModelBasic::CreateCSPSO()
{
  CreateCSRootSignature();

  Microsoft::WRL::ComPtr<IDxcBlob> csBlob = m_dx12_->CompileShader(L"resources/shaders/Skinning.CS.hlsl", L"cs_6_0");

  D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
  computePipelineStateDesc.pRootSignature = csRootSignature_.Get();
  computePipelineStateDesc.CS = { .pShaderBytecode = csBlob->GetBufferPointer(), .BytecodeLength = csBlob->GetBufferSize() };

  HRESULT hr = m_dx12_->GetDevice()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&csPipelineState_));
  assert(SUCCEEDED(hr));
}
