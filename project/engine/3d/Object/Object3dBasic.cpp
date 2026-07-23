#include "Object3dBasic.h"
#include "DX12Basic.h"
#include "Camera.h"
#include "SrvManager.h"
#include "PostEffectManager.h"
#include "ShadowRenderer.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "DebugCamera.h"
#endif

namespace Tako {

std::unique_ptr<Object3dBasic> Object3dBasic::instance_ = nullptr;

Object3dBasic* Object3dBasic::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<Object3dBasic>(Token{});
	}
	return instance_.get();
}

void Object3dBasic::Initialize(DX12Basic* dx12)
{
	dx12_ = dx12;

	isDebug_ = false;

	CreatePSO();
	CreateInstancedPSO();
	CreateTransparentPSO();

	// ライトの生成と初期化
	light_ = std::make_unique<Light>();
	light_->Initialize(dx12_);
	
}

void Object3dBasic::Update()
{
	if (isDebug_)
	{
#ifdef _DEBUG
		debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMat();
		camera_->SetViewProjectionMatrix(debugViewProjectionMatrix_);
#endif
	} else
	{
		viewProjectionMatrix_ = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
		camera_->SetViewProjectionMatrix(viewProjectionMatrix_);
	}
	
  light_->Update();
}

void Object3dBasic::Finalize()
{
	light_.reset();

	instance_.reset();
}

void Object3dBasic::SetCommonRenderSetting()
{
	// ルートシグネチャの設定
	dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// パイプラインステートの設定
	dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

	// トポロジの設定
	dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ライトの描画設定
	light_->PreDraw();

	// ShadowRenderer のリソース設定（ルートパラメータ9と10）
	ShadowRenderer::GetInstance()->SetShadowForMainPass();
}

void Object3dBasic::SetTransparentRenderSetting()
{
	// ルートシグネチャは通常描画用と共有
	dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// 半透明描画用パイプラインステートの設定 (CullMode=NONE, DepthWriteMask=ZERO)
	dx12_->GetCommandList()->SetPipelineState(transparentPipelineState_.Get());

	// トポロジの設定
	dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ライトの描画設定
	light_->PreDraw();

	// ShadowRenderer のリソース設定（ルートパラメータ9と10）
	ShadowRenderer::GetInstance()->SetShadowForMainPass();
}

void Object3dBasic::SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity)
{
	light_->SetDirectionalLight(direction, color, lightType, intensity);
}

void Object3dBasic::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index)
{
	light_->SetPointLight(position, color, intensity, radius, decay, enable, index);
}

void Object3dBasic::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index)
{
	light_->SetSpotLight(position, direction, color, intensity, distance, decay, cosAngle, enable, index);
}

void Object3dBasic::CreateRootSignature()
{
	HRESULT hr;

	// rootSignature の生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Sampler の設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2]{};
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大 LOD
	samplerDesc[0].ShaderRegister = 0; // レジスタ番号
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	
	// シャドウマップ用比較サンプラー
	samplerDesc[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 比較サンプリング
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 深度比較
	samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[1].ShaderRegister = 1; // レジスタ番号（s1）
	samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = samplerDesc;
	descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

	// DescriptorRange の設定。
  // Texture
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

  // PointLight
	D3D12_DESCRIPTOR_RANGE descriptorRangeForPointLight[1] = {};
	descriptorRangeForPointLight[0].BaseShaderRegister = 1; // レジスタ番号
	descriptorRangeForPointLight[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRangeForPointLight[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
	descriptorRangeForPointLight[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

  // SpotLight
	D3D12_DESCRIPTOR_RANGE descriptorRangeForSpotLight[1] = {};
	descriptorRangeForSpotLight[0].BaseShaderRegister = 2; // レジスタ番号
	descriptorRangeForSpotLight[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRangeForSpotLight[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
	descriptorRangeForSpotLight[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

  // EnvironmentMap
  D3D12_DESCRIPTOR_RANGE descriptorRangeForEnvironmentMap[1] = {};
  descriptorRangeForEnvironmentMap[0].BaseShaderRegister = 3; // レジスタ番号
  descriptorRangeForEnvironmentMap[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForEnvironmentMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
  descriptorRangeForEnvironmentMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

  // ShadowMap
  D3D12_DESCRIPTOR_RANGE descriptorRangeForShadowMap[1] = {};
  descriptorRangeForShadowMap[0].BaseShaderRegister = 4; // レジスタ番号（t4）
  descriptorRangeForShadowMap[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForShadowMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV を使う
  descriptorRangeForShadowMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offset を自動計算

	// RootParameter の設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[11] = {};

	// Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	// TransformationMatrix
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド 

	// Texture
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // ディスクリプタレンジを設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // レンジの数

	// DirectionalLight
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド

	// GPU Camera
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[4].Descriptor.ShaderRegister = 2; // レジスタ番号とバインド

	// PointLight
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[5].DescriptorTable.pDescriptorRanges = descriptorRangeForPointLight; // ディスクリプタレンジを設定
	rootParameters[5].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForPointLight); // レンジの数

	// SpotLight
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[6].DescriptorTable.pDescriptorRanges = descriptorRangeForSpotLight; // ディスクリプタレンジを設定
	rootParameters[6].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForSpotLight); // レンジの数

	// LightNum
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[7].Descriptor.ShaderRegister = 3; // レジスタ番号とバインド

  // EnvironmentMap
  rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[8].DescriptorTable.pDescriptorRanges = descriptorRangeForEnvironmentMap; // ディスクリプタレンジを設定
  rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForEnvironmentMap); // レンジの数

  // ShadowConstants（b4）
  rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
  rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 頂点・ピクセルシェーダーで使う
  rootParameters[9].Descriptor.ShaderRegister = 4; // レジスタ番号とバインド

  // ShadowMap（t4）
  rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
  rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
  rootParameters[10].DescriptorTable.pDescriptorRanges = descriptorRangeForShadowMap; // ディスクリプタレンジを設定
  rootParameters[10].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForShadowMap); // レンジの数

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
#ifdef _DEBUG
		DebugUIManager::GetInstance()->AddLog(
      static_cast<char*>(errorBlob->GetBufferPointer()),
      DebugUIManager::LogType::Error);
#endif

		assert(false);
	}

	hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
	assert(SUCCEEDED(hr));

}

void Object3dBasic::CreatePSO()
{
	HRESULT hr;

	// RootSignature の生成
	CreateRootSignature();

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
  blendDesc.RenderTarget[0].BlendEnable = true;
  blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// shader のコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Object3d.VS.hlsl"), L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Object3d.PS.hlsl"), L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// depth の機能を有効化にする
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 深度の比較方法
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

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
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// 実際に生成
	hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void Object3dBasic::CreateTransparentPSO()
{
	HRESULT hr;

	// RootSignature は通常描画用 (rootSignature_) を共有する — CreatePSO で生成済み

	// InputLayout (通常描画と同じ: POSITION, TEXCOORD, NORMAL)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState (通常描画と同じアルファブレンド)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState (差分: 両面描画)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 半透明描画では裏面も表示する (CullMode=NONE)
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// shader (通常描画と同じものを使用)
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Object3d.VS.hlsl"), L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Object3d.PS.hlsl"), L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState (差分: 深度書き込み無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	// 半透明描画では深度書き込みを無効化 (Z-fighting 防止 + 後続の半透明オブジェクトと正しく合成)
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

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
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// 実際に生成
	hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&transparentPipelineState_));
	assert(SUCCEEDED(hr));
}

void Object3dBasic::SetInstancedRenderSetting()
{
	// インスタンシング用ルートシグネチャの設定
	dx12_->GetCommandList()->SetGraphicsRootSignature(instancedRootSignature_.Get());

	// インスタンシング用パイプラインステートの設定
	dx12_->GetCommandList()->SetPipelineState(instancedPipelineState_.Get());

	// トポロジの設定
	dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ライトの描画設定
	light_->PreDraw();
	
	// ShadowRenderer のリソース設定
	ShadowRenderer::GetInstance()->SetShadowForMainPass();
}

void Object3dBasic::CreateInstancedRootSignature()
{
	HRESULT hr;

	// rootSignature の生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Sampler の設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2]{};
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[0].ShaderRegister = 0;
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
	// シャドウマップ用比較サンプラー
	samplerDesc[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[1].ShaderRegister = 1;
	samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = samplerDesc;
	descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

	// DescriptorRange の設定
	// テクスチャ（t0）
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// PointLight（t1）
	D3D12_DESCRIPTOR_RANGE descriptorRangeForPointLight[1] = {};
	descriptorRangeForPointLight[0].BaseShaderRegister = 1;
	descriptorRangeForPointLight[0].NumDescriptors = 1;
	descriptorRangeForPointLight[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForPointLight[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// SpotLight（t2）
	D3D12_DESCRIPTOR_RANGE descriptorRangeForSpotLight[1] = {};
	descriptorRangeForSpotLight[0].BaseShaderRegister = 2;
	descriptorRangeForSpotLight[0].NumDescriptors = 1;
	descriptorRangeForSpotLight[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForSpotLight[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// EnvironmentMap（t3）
	D3D12_DESCRIPTOR_RANGE descriptorRangeForEnvironmentMap[1] = {};
	descriptorRangeForEnvironmentMap[0].BaseShaderRegister = 3;
	descriptorRangeForEnvironmentMap[0].NumDescriptors = 1;
	descriptorRangeForEnvironmentMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForEnvironmentMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ShadowMap（t4）
	D3D12_DESCRIPTOR_RANGE descriptorRangeForShadowMap[1] = {};
	descriptorRangeForShadowMap[0].BaseShaderRegister = 4;
	descriptorRangeForShadowMap[0].NumDescriptors = 1;
	descriptorRangeForShadowMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForShadowMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// インスタンスデータ（t5）
	D3D12_DESCRIPTOR_RANGE descriptorRangeInstance[1] = {};
	descriptorRangeInstance[0].BaseShaderRegister = 5;
	descriptorRangeInstance[0].NumDescriptors = 1;
	descriptorRangeInstance[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeInstance[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter の設定
	D3D12_ROOT_PARAMETER rootParameters[12] = {};

	// Material（b0 - pixel）
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// ViewProjection（b0 - vertex）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// Texture（t0）
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// DirectionalLight（b1）
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// GPU Camera（b2）
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	// PointLight（t1）
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].DescriptorTable.pDescriptorRanges = descriptorRangeForPointLight;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForPointLight);

	// SpotLight（t2）
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].DescriptorTable.pDescriptorRanges = descriptorRangeForSpotLight;
	rootParameters[6].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForSpotLight);

	// LightNum（b3）
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].Descriptor.ShaderRegister = 3;

	// EnvironmentMap（t3）
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[8].DescriptorTable.pDescriptorRanges = descriptorRangeForEnvironmentMap;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForEnvironmentMap);

	// ShadowConstants（b4）
	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[9].Descriptor.ShaderRegister = 4;

	// ShadowMap（t4）
	rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[10].DescriptorTable.pDescriptorRanges = descriptorRangeForShadowMap;
	rootParameters[10].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForShadowMap);

	// インスタンスデータ（t5）
	rootParameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[11].DescriptorTable.pDescriptorRanges = descriptorRangeInstance;
	rootParameters[11].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeInstance);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
#ifdef _DEBUG
		DebugUIManager::GetInstance()->AddLog(
      static_cast<char*>(errorBlob->GetBufferPointer()),
      DebugUIManager::LogType::Error);
#endif

		assert(false);
	}

	hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(instancedRootSignature_.GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void Object3dBasic::CreateInstancedPSO()
{
	HRESULT hr;

	// RootSignature の生成
	CreateInstancedRootSignature();

	// InputLayout（頂点レイアウト）
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Object3dInstanced.VS.hlsl"), L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"Object3dInstanced.PS.hlsl"), L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSO の生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = instancedRootSignature_.Get();
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
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// 実際に生成
	hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&instancedPipelineState_));
	assert(SUCCEEDED(hr));
}

} // namespace Tako

