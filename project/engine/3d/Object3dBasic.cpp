#include "Object3dBasic.h"
#include "DX12Basic.h"
#include "Logger.h"
#include "Camera.h"
#include "SrvManager.h"
#include "PostEffectManager.h"

#ifdef _DEBUG
#include "DebugCamera.h"
#endif

Object3dBasic* Object3dBasic::instance_ = nullptr;

Object3dBasic* Object3dBasic::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new Object3dBasic();
	}
	return instance_;
}

void Object3dBasic::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

	isDebug_ = false;

	CreatePSO();
	CreateShadowRootSignature();  // シャドウ用ルートシグネチャの作成
	CreateShadowPSO();

	// ライトの生成と初期化
	light_ = new Light();
	light_->Initialize(m_dx12_);
	
	// シャドウマップの生成と初期化
	shadowMap_ = new ShadowMap();
	shadowMap_->Initialize(m_dx12_);
	
	// シャドウ用定数バッファの作成
	shadowConstantBuffer_ = m_dx12_->MakeBufferResource(sizeof(ShadowConstants));
	shadowConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&shadowConstantData_));
	
	// 初期値設定
	shadowConstantData_->lightViewProj = Mat4x4::MakeIdentity();
	shadowConstantData_->shadowBias = 0.0001f;
	shadowConstantData_->enableShadow = shadowEnabled_ ? 1 : 0;
	shadowConstantData_->shadowMapSize = {2048.0f, 2048.0f};
	shadowConstantData_->normalOffsetBias = 0.01f;
	shadowConstantData_->pcfKernelSize = 3.0f;
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
  
  // シャドウマップの更新
  if (shadowEnabled_) {
      light_->UpdateDirectionalLightShadowMatrices();
      shadowConstantData_->lightViewProj = light_->GetDirectionalLight().viewProjMatrix;
      shadowConstantData_->enableShadow = 1;
      shadowConstantData_->shadowMapSize = {static_cast<float>(shadowMap_->GetShadowMapSize()), static_cast<float>(shadowMap_->GetShadowMapSize())};
      shadowConstantData_->normalOffsetBias = 0.01f;  // TODO: 動的に変更可能にする
      shadowConstantData_->pcfKernelSize = static_cast<float>(shadowMap_->GetPCFKernelSize());
      shadowMap_->SetLightViewProjectionMatrix(light_->GetDirectionalLight().viewProjMatrix);
  } else {
      shadowConstantData_->enableShadow = 0;
  }
}

void Object3dBasic::Finalize()
{
	delete light_;
	
	if (shadowMap_) {
		shadowMap_->Finalize();
		delete shadowMap_;
	}

	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Object3dBasic::SetCommonRenderSetting()
{
	// ルートシグネチャの設定
	m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// パイプラインステートの設定
	m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

	// トポロジの設定
	m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ライトの描画設定
	light_->PreDraw();
	
	// シャドウ定数バッファの設定（ルートパラメータ9、レジスタb4）
	if (shadowConstantBuffer_) {
		m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(9, shadowConstantBuffer_->GetGPUVirtualAddress());
	}
	
	// シャドウマップの設定（ルートパラメータ10、テクスチャt4）
	// シャドウマップレンダリング中はSRV設定をスキップ
	if (!isRenderingShadowMap_ && shadowMap_) {
#ifdef _DEBUG
		OutputDebugStringA("Object3dBasic::SetCommonRenderSetting() - Setting shadow map as shader resource\n");
#endif
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(10, shadowMap_->GetSrvIndex());
	}
#ifdef _DEBUG
	else if (isRenderingShadowMap_) {
		OutputDebugStringA("Object3dBasic::SetCommonRenderSetting() - Skipping shadow map SRV (rendering shadow map)\n");
	}
#endif
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

void Object3dBasic::SetShadowQuality(ShadowMap::ShadowQuality quality)
{
	if (shadowMap_) {
		shadowMap_->SetShadowQuality(quality);
		// 定数バッファを更新
		shadowConstantData_->shadowMapSize = {static_cast<float>(shadowMap_->GetShadowMapSize()), static_cast<float>(shadowMap_->GetShadowMapSize())};
		shadowConstantData_->pcfKernelSize = static_cast<float>(shadowMap_->GetPCFKernelSize());
	}
}

void Object3dBasic::SetShadowMapSize(uint32_t size)
{
	if (shadowMap_) {
		shadowMap_->SetShadowMapSize(size);
		// 定数バッファを更新
		shadowConstantData_->shadowMapSize = {static_cast<float>(shadowMap_->GetShadowMapSize()), static_cast<float>(shadowMap_->GetShadowMapSize())};
	}
}

void Object3dBasic::SetPCFKernelSize(int kernelSize)
{
	if (shadowMap_) {
		shadowMap_->SetPCFKernelSize(kernelSize);
		// 定数バッファを更新
		shadowConstantData_->pcfKernelSize = static_cast<float>(shadowMap_->GetPCFKernelSize());
	}
}

void Object3dBasic::SetNormalOffsetBias(float bias)
{
	if (shadowMap_) {
		shadowMap_->SetNormalOffsetBias(bias);
		// 定数バッファを更新
		shadowConstantData_->normalOffsetBias = bias;
	}
}

void Object3dBasic::CreateRootSignature()
{
	HRESULT hr;

	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2]{};
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大LOD
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

	// DescriptorRangeの設定。
  // Texture
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // PointLight
	D3D12_DESCRIPTOR_RANGE descriptorRangeForPointLight[1] = {};
	descriptorRangeForPointLight[0].BaseShaderRegister = 1; // レジスタ番号
	descriptorRangeForPointLight[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRangeForPointLight[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForPointLight[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // SpotLight
	D3D12_DESCRIPTOR_RANGE descriptorRangeForSpotLight[1] = {};
	descriptorRangeForSpotLight[0].BaseShaderRegister = 2; // レジスタ番号
	descriptorRangeForSpotLight[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRangeForSpotLight[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForSpotLight[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // EnvironmentMap
  D3D12_DESCRIPTOR_RANGE descriptorRangeForEnvironmentMap[1] = {};
  descriptorRangeForEnvironmentMap[0].BaseShaderRegister = 3; // レジスタ番号
  descriptorRangeForEnvironmentMap[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForEnvironmentMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangeForEnvironmentMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

  // ShadowMap
  D3D12_DESCRIPTOR_RANGE descriptorRangeForShadowMap[1] = {};
  descriptorRangeForShadowMap[0].BaseShaderRegister = 4; // レジスタ番号（t4）
  descriptorRangeForShadowMap[0].NumDescriptors = 1; // ディスクリプタ数
  descriptorRangeForShadowMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
  descriptorRangeForShadowMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// RootParameterの設定。複数設定できるので配列
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
		Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
	signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf());
	assert(SUCCEEDED(hr));

}

void Object3dBasic::CreateShadowRootSignature()
{
	HRESULT hr;

	// shadowRootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定（シャドウマップ生成時は不要だが、最小限の設定）
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

	// RootParameterの設定（シャドウマップ生成に必要な最小限のパラメータ）
	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	// Parameter 0: TransformationMatrix (b0)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// Parameter 1: ShadowConstants (b4)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 4;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(shadowRootSignature_.GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void Object3dBasic::CreatePSO()
{
	HRESULT hr;

	// RootSignatureの生成
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

	// shaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// depthの機能を有効化にする
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 深度の比較方法
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

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
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// 実際に生成
	hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void Object3dBasic::CreateShadowPSO()
{
	HRESULT hr;
	
	// InputLayout（通常のObject3dと同じ）
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState（深度のみなので不要だが設定）
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = 0; // カラー出力しない

	// RasterizerState（フロントフェースカリング）
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; // シャドウアクネ対策
	rasterizerDesc.DepthBias = 100000; // 深度バイアス
	rasterizerDesc.SlopeScaledDepthBias = 1.0f; // スロープスケール深度バイアス

	// シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/ShadowMap.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	
	// ピクセルシェーダーは不要（深度のみ）

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	// シャドウPSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = shadowRootSignature_.Get(); // シャドウ用ルートシグネチャを使用
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { nullptr, 0 }; // ピクセルシェーダーなし
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.NumRenderTargets = 0; // カラーターゲットなし
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// PSOを生成
	hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&shadowPipelineState_));
	assert(SUCCEEDED(hr));
}

void Object3dBasic::SetShadowRenderSetting()
{
	// シャドウ用ルートシグネチャの設定
	m_dx12_->GetCommandList()->SetGraphicsRootSignature(shadowRootSignature_.Get());
	
	// シャドウ用パイプラインステートの設定
	m_dx12_->GetCommandList()->SetPipelineState(shadowPipelineState_.Get());
	
	// トポロジの設定
	m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// シャドウ定数バッファの設定（パラメータ1、レジスタb4）
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, shadowConstantBuffer_->GetGPUVirtualAddress());
}

void Object3dBasic::BeginShadowMapRender()
{
	if (!shadowEnabled_ || !shadowMap_) return;
	
	// シャドウマップレンダリング中フラグを設定
	isRenderingShadowMap_ = true;
	
	// 現在のレンダーターゲットとデプスバッファを保存
	savedRTVHandle_ = PostEffectManager::GetInstance()->GetCurrentRTVHandle();
	savedDSVHandle_ = m_dx12_->GetDSVHeapHandleStart();
	hasSavedRenderTargets_ = true;
	
	shadowMap_->BeginShadowMapRender();
	SetShadowRenderSetting();
}

void Object3dBasic::EndShadowMapRender()
{
	if (!shadowEnabled_ || !shadowMap_) return;
	
	shadowMap_->EndShadowMapRender();
	
	// シャドウマップレンダリング中フラグをクリア
	isRenderingShadowMap_ = false;
	
	// 元のレンダーターゲットとデプスバッファを復元
	if (hasSavedRenderTargets_) {
		m_dx12_->GetCommandList()->OMSetRenderTargets(1, &savedRTVHandle_, FALSE, &savedDSVHandle_);
		hasSavedRenderTargets_ = false;
	}
	
	// ビューポートとシザー矩形を元に戻す
	m_dx12_->SetViewPort();
}
