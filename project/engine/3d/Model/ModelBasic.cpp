#include "ModelBasic.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  void ModelBasic::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    directoryFolderName_ = "resources";

    modelFolderName_ = "Model";

    CreateCSPSO();
  }

  void ModelBasic::SetSkinningCSSetting()
  {
    dx12_->GetCommandList()->SetComputeRootSignature(csRootSignature_.Get());
    dx12_->GetCommandList()->SetPipelineState(csPipelineState_.Get());
  }

  void ModelBasic::CreateCSRootSignature()
  {
    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // DescriptorRange の設定。
    D3D12_DESCRIPTOR_RANGE descriptorRangeForPalette[1] = {};
    descriptorRangeForPalette[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRangeForPalette[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForPalette[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRangeForPalette[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForVertexInput[1] = {};
    descriptorRangeForVertexInput[0].BaseShaderRegister = 1; // レジスタ番号
    descriptorRangeForVertexInput[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForVertexInput[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRangeForVertexInput[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForInfluence[1] = {};
    descriptorRangeForInfluence[0].BaseShaderRegister = 2; // レジスタ番号
    descriptorRangeForInfluence[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForInfluence[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
    descriptorRangeForInfluence[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_DESCRIPTOR_RANGE descriptorRangeForVertexOutput[1] = {};
    descriptorRangeForVertexOutput[0].BaseShaderRegister = 0; // レジスタ番号
    descriptorRangeForVertexOutput[0].NumDescriptors = 1; // ディスクリプタ数
    descriptorRangeForVertexOutput[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // UAV を使う
    descriptorRangeForVertexOutput[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

    D3D12_ROOT_PARAMETER rootParameters[5] = {};

    // Palette
    rootParameters[kPaletteParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[kPaletteParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[kPaletteParam].DescriptorTable.pDescriptorRanges = descriptorRangeForPalette; // ディスクリプタレンジを設定
    rootParameters[kPaletteParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForPalette); // レンジの数

    // VertexInput
    rootParameters[kVertexInputParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[kVertexInputParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[kVertexInputParam].DescriptorTable.pDescriptorRanges = descriptorRangeForVertexInput; // ディスクリプタレンジを設定
    rootParameters[kVertexInputParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForVertexInput); // レンジの数

    // Influence
    rootParameters[kInfluenceParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[kInfluenceParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[kInfluenceParam].DescriptorTable.pDescriptorRanges = descriptorRangeForInfluence; // ディスクリプタレンジを設定
    rootParameters[kInfluenceParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInfluence); // レンジの数

    // VertexOutput
    rootParameters[kVertexOutputParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
    rootParameters[kVertexOutputParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[kVertexOutputParam].DescriptorTable.pDescriptorRanges = descriptorRangeForVertexOutput; // ディスクリプタレンジを設定
    rootParameters[kVertexOutputParam].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForVertexOutput); // レンジの数

    // SkinningInfo
    rootParameters[kSkinningInfoParam].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[kSkinningInfoParam].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 全てのシェーダーで使う
    rootParameters[kSkinningInfoParam].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        reinterpret_cast<char*>(errorBlob->GetBufferPointer()),
        DebugUIManager::LogType::Error);
#endif

      assert(false);
    }

    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(csRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    DebugUIManager::GetInstance()->AddLog("CSRootSignature Created", DebugUIManager::LogType::Info);
#endif

  }

  void ModelBasic::CreateCSPSO()
  {
    CreateCSRootSignature();

    Microsoft::WRL::ComPtr<IDxcBlob> csBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Skinning.CS.hlsl"), L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
    computePipelineStateDesc.pRootSignature = csRootSignature_.Get();
    computePipelineStateDesc.CS = { .pShaderBytecode = csBlob->GetBufferPointer(), .BytecodeLength = csBlob->GetBufferSize() };

    HRESULT hr = dx12_->GetDevice()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&csPipelineState_));
    assert(SUCCEEDED(hr));
  }

} // namespace Tako
