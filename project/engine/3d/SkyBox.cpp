#include "SkyBox.h"
#include "Object3dBasic.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "Mat4x4Func.h"
#include "Camera.h"
#include "Logger.h"

void SkyBox::Initialize(const std::string& texturePath)
{
  // Transformの初期化
  transform_.scale = { 500.0f, 500.0f, 500.0f };
  transform_.rotate = { 0.0f, 0.0f, 0.0f };
  transform_.translate = { 0.0f, 0.0f, 0.0f };

  // 行列の初期化
  viewProjectionMatrix_ = Mat4x4::MakeIdentity();
  worldMatrix_ = Mat4x4::MakeIdentity();
  wvpMatrix_ = Mat4x4::MakeIdentity();

  // DX12の基本情報を保存
  m_dx12_ = Object3dBasic::GetInstance()->GetDX12Basic();

  // パイプラインステートを生成
  CreatePSO();
  // 頂点データを生成
  CreateVertexData();
  // インデックスデータを生成
  CreateIndexData();
  // マテリアルデータを生成
  CreateMaterialData();
  // 座標変換行列データを生成
  CreateTransformationMatrixData();

  // テクスチャインデックスを保存
  textureIndex_ = TextureManager::GetInstance()->GetSRVIndex(texturePath);
}

void SkyBox::Update()
{
  viewProjectionMatrix_ = (*Object3dBasic::GetInstance()->GetCamera())->GetViewProjectionMatrix();
  worldMatrix_ = Mat4x4::MakeAffine(transform_.scale, transform_.rotate, transform_.translate);
  wvpMatrix_ = Mat4x4::Multiply(worldMatrix_, viewProjectionMatrix_);

  // リソースにデータを書き込む
  transformationMatrixData_->WVP = wvpMatrix_;
}

void SkyBox::Draw()
{
  // rootSignatureを設定
  m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

  // パイプラインステートを設定
  m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

  // プリミティブトポロジの設定
  m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // 頂点バッファビューの設定
  m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

  // インデックスバッファビューの設定
  m_dx12_->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

  // 座標変換行列CBufferの場所を設定
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixResource_->GetGPUVirtualAddress());

  // SRVのDescriptorTableを設定,テクスチャを指定
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, textureIndex_);

  // マテリアルCBufferの場所を設定
  m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(2, materialResource_->GetGPUVirtualAddress());

  // 描画
  m_dx12_->GetCommandList()->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void SkyBox::CreateRootSignature()
{
  HRESULT hr;

  // rootSignatureの生成
  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
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
  D3D12_DESCRIPTOR_RANGE textureDescriptorRange[1] = {};
  textureDescriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
  textureDescriptorRange[0].NumDescriptors = 1; // ディスクリプタ数
  textureDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  textureDescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // RootParameterの設定。
  D3D12_ROOT_PARAMETER rootParameters[3] = {};
  // TransformationMatrix
  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
  rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

  // Texture
  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[1].DescriptorTable.pDescriptorRanges = textureDescriptorRange; // ディスクリプタレンジを設定
  rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(textureDescriptorRange); // レンジの数

  // Material
  rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[2].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

  descriptionRootSignature.pParameters = rootParameters;
  descriptionRootSignature.NumParameters = _countof(rootParameters);

  Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

  // ルートシグネチャをシリアライズ
  hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
  if (FAILED(hr))
  {
    Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
    assert(false);
  }
  // ルートシグネチャを生成
  hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
  assert(SUCCEEDED(hr));
}

void SkyBox::CreatePSO()
{
  HRESULT hr;

  // RootSignatureの生成
  CreateRootSignature();

  // InputLayout
  D3D12_INPUT_ELEMENT_DESC inputElementDescs[1]{};
  inputElementDescs[0].SemanticName = "POSITION";
  inputElementDescs[0].SemanticIndex = 0;
  inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = inputElementDescs;
  inputLayoutDesc.NumElements = _countof(inputElementDescs);

  // BlendState
  D3D12_BLEND_DESC blendDesc{};
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  // RasterizerState
  D3D12_RASTERIZER_DESC rasterizerDesc{};
  rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 塗りつぶしモード
  rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; // 裏面を描画しない

  // shaderのコンパイル
  Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/SkyBox.VS.hlsl", L"vs_6_0");
  assert(vertexShaderBlob != nullptr);

  Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/SkyBox.PS.hlsl", L"ps_6_0");
  assert(pixelShaderBlob != nullptr);

  // DepthStencilStateの設定
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
  depthStencilDesc.DepthEnable = true;                           // 深度バッファを有効にする
  depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度値を書き込まない
  depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 深度比較関数

  // PSOの設定
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
  pipelineStateDesc.pRootSignature = rootSignature_.Get(); // ルートシグネチャ
  pipelineStateDesc.InputLayout = inputLayoutDesc;         // 入力レイアウト
  pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // プリミティブトポロジ
  pipelineStateDesc.RasterizerState = rasterizerDesc;     // ラスタライザーステート
  pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() }; // 頂点シェーダー
  pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() }; // ピクセルシェーダー
  pipelineStateDesc.BlendState = blendDesc;               // ブレンドステート
  pipelineStateDesc.DepthStencilState = depthStencilDesc; // 深度ステンシルステート
  pipelineStateDesc.NumRenderTargets = 1;                 // レンダーターゲット数
  pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // レンダーターゲットフォーマット
  pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // サンプルマスク
  pipelineStateDesc.SampleDesc.Count = 1;                // サンプル数
  pipelineStateDesc.DepthStencilState = depthStencilDesc; // 深度ステンシルステート
  pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // 深度ステンシルフォーマット

  // PSOの生成
  hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
  assert(SUCCEEDED(hr));

}

void SkyBox::CreateVertexData()
{
  // 頂点リソースを生成
  vertexResource_ = m_dx12_->MakeBufferResource(sizeof(VertexData) * 24);

  // 頂点リソースをマップ
  vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

  // 頂点データを設定
  // 右面。インデックス[0, 1, 2][2, 1, 3]
  vertexData_[0] = { Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[1] = { Vector4(1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[2] = { Vector4(1.0f, -1.0f, 1.0f, 1.0f) };
  vertexData_[3] = { Vector4(1.0f, -1.0f, -1.0f, 1.0f) };
  // 左面。インデックス[4, 5, 6][6, 5, 7]
  vertexData_[4] = { Vector4(-1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[5] = { Vector4(-1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[6] = { Vector4(-1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[7] = { Vector4(-1.0f, -1.0f, 1.0f, 1.0f) };
  // 前面。インデックス[8, 9, 10][10, 9, 11]
  vertexData_[8] = { Vector4(-1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[9] = { Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[10] = { Vector4(-1.0f, -1.0f, 1.0f, 1.0f) };
  vertexData_[11] = { Vector4(1.0f, -1.0f, 1.0f, 1.0f) };
  // 後面。インデックス[12, 13, 14][14, 13, 15]
  vertexData_[12] = { Vector4(1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[13] = { Vector4(-1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[14] = { Vector4(1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[15] = { Vector4(-1.0f, -1.0f, -1.0f, 1.0f) };
  // 上面。インデックス[16, 17, 18][18, 17, 19]
  vertexData_[16] = { Vector4(-1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[17] = { Vector4(1.0f, 1.0f, -1.0f, 1.0f) };
  vertexData_[18] = { Vector4(-1.0f, 1.0f, 1.0f, 1.0f) };
  vertexData_[19] = { Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
  // 下面。インデックス[20, 21, 22][22, 21, 23]
  vertexData_[20] = { Vector4(1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[21] = { Vector4(-1.0f, -1.0f, -1.0f, 1.0f) };
  vertexData_[22] = { Vector4(1.0f, -1.0f, 1.0f, 1.0f) };
  vertexData_[23] = { Vector4(-1.0f, -1.0f, 1.0f, 1.0f) };

  // 頂点バッファビューを作成する
  vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
  vertexBufferView_.SizeInBytes = sizeof(VertexData) * 24;
  vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void SkyBox::CreateIndexData()
{
  // インデックスリソースを生成
  indexResource_ = m_dx12_->MakeBufferResource(sizeof(uint32_t) * 36);

  // インデックスリソースをマップ
  indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

  // インデックスデータを設定
  // 各面のインデックスデータを設定
  indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
  indexData_[3] = 2; indexData_[4] = 1; indexData_[5] = 3;
  indexData_[6] = 4; indexData_[7] = 5; indexData_[8] = 6;
  indexData_[9] = 6; indexData_[10] = 5; indexData_[11] = 7;
  indexData_[12] = 8; indexData_[13] = 9; indexData_[14] = 10;
  indexData_[15] = 10; indexData_[16] = 9; indexData_[17] = 11;
  indexData_[18] = 12; indexData_[19] = 13; indexData_[20] = 14;
  indexData_[21] = 14; indexData_[22] = 13; indexData_[23] = 15;
  indexData_[24] = 16; indexData_[25] = 17; indexData_[26] = 18;
  indexData_[27] = 18; indexData_[28] = 17; indexData_[29] = 19;
  indexData_[30] = 20; indexData_[31] = 21; indexData_[32] = 22;
  indexData_[33] = 22; indexData_[34] = 21; indexData_[35] = 23;

  // indexBufferViewを作成
  indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
  indexBufferView_.SizeInBytes = sizeof(uint32_t) * 36;
  indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void SkyBox::CreateMaterialData()
{
  // マテリアルリソースを生成
  materialResource_ = m_dx12_->MakeBufferResource(sizeof(Material));
  // マテリアルリソースをマップ
  materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
  // マテリアルデータの初期値を書き込む
  materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void SkyBox::CreateTransformationMatrixData()
{
  // 座標変換行列リソースを生成
  transformationMatrixResource_ = m_dx12_->MakeBufferResource(sizeof(TransformationMatrix));
  // 座標変換行列リソースをマップ
  transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
  // 座標変換行列データの初期値を書き込む
  transformationMatrixData_->WVP = Mat4x4::MakeIdentity();
}