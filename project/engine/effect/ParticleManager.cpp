#include "ParticleManager.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Object3dBasic.h"
#include "Camera.h"
#include "Logger.h"
#include "Mat4x4Func.h"
#include "WinApp.h"
#include <numbers>


ParticleManager* ParticleManager::instance_ = nullptr;

ParticleManager* ParticleManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new ParticleManager();
	}
	return instance_;
}

void ParticleManager::Initialize(DX12Basic* dx12, Camera* camera)
{
	m_dx12_ = dx12;

	// カメラの設定
	m_camera_ = camera;

	srvManager_ = SrvManager::GetInstance();

	// ランダムエンジンの初期化
	randomEngine_.seed(seedGenerator());

	// PSOの生成
	CreatePSO();

	// 頂点データの生成
	CreateVertexData();

	// マテリアルデータの生成
	CreateMaterialData();
}

void ParticleManager::Update()
{
	Matrix4x4 cameraMatrix = Mat4x4::MakeAffine({ 1.0f,1.0f,1.0f }, m_camera_->GetRotate(), m_camera_->GetTranslate());

	Matrix4x4 viewMatrix = Mat4x4::Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = m_camera_->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Mat4x4::Multiply(viewMatrix, projectionMatrix);

	// ビルボード行列の生成
	Matrix4x4 backToFrontMatrix = Mat4x4::MakeRotateY(std::numbers::pi_v<float>);
	Matrix4x4 billboardMatrix{};
	if (isBillboard_)
	{
		billboardMatrix = Mat4x4::Multiply(backToFrontMatrix, cameraMatrix);
		billboardMatrix.m[3][0] = 0.0f;  //平行移動成分はいらない
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
	} 
	else if (isBillboard_ == false)
	{
		billboardMatrix = Mat4x4::MakeIdentity();
	}


	for (auto& group : particleGroups)
	{
		for (std::list<Particle>::iterator particleIterator = group.second.particleList.begin(); particleIterator != group.second.particleList.end();)
		{
			if ((*particleIterator).lifeTime <= (*particleIterator).currentTime)
			{
				//　生存期間が過ぎたParticleはlistから消す
				particleIterator = group.second.particleList.erase(particleIterator);
				continue;
			}

			if (group.second.instanceCount < kNumMaxInstance_) {

				//　速度の更新
				(*particleIterator).transform.translate = (*particleIterator).transform.translate + (*particleIterator).velocity * kDeltaTime_;
				(*particleIterator).currentTime += kDeltaTime_;

				Matrix4x4 worldMatrix = Mat4x4::Multiply(billboardMatrix, Mat4x4::MakeAffine((*particleIterator).transform.scale, (*particleIterator).transform.rotate, (*particleIterator).transform.translate));
				Matrix4x4 worldviewProjectionMatrix = Mat4x4::Multiply(worldMatrix, viewProjectionMatrix);

				group.second.pParticleDataForGPU[group.second.instanceCount].WVP = worldviewProjectionMatrix;
				group.second.pParticleDataForGPU[group.second.instanceCount].world = worldMatrix;

				//　色の設定
				group.second.pParticleDataForGPU[group.second.instanceCount].color = (*particleIterator).color;

				//　徐々に消えていくようにアルファ値を設定
				float alpha = 1.0f - ((*particleIterator).currentTime / (*particleIterator).lifeTime);
				alpha = (alpha < 0.0f) ? 0.0f : alpha;  // アルファが0以下にならないようにする
				group.second.pParticleDataForGPU[group.second.instanceCount].color.w = alpha;

				++group.second.instanceCount;
			}
			++particleIterator;
		}
	}
}

void ParticleManager::Draw()
{
	ID3D12GraphicsCommandList* commandList = m_dx12_->GetCommandList();

	// ルートシグネチャの設定
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// パイプラインステートの設定
	commandList->SetPipelineState(pipelineState_.Get());

	// プリミティブトポロジを設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VBVを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// materialの設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 全てのパーティクルグループの処理
	for (auto& group : particleGroups)
	{
		if (group.second.instanceCount == 0) { continue; }

		// テクスチャの設定
		commandList->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(group.second.texture.textureIndex));

		// ParticleDataの設定
		commandList->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.second.instancingSrvIndex));

		// 描画
		commandList->DrawInstanced(6, group.second.instanceCount, 0, 0);

		// インスタンス数をリセット
		group.second.instanceCount = 0;
	}
}

void ParticleManager::Finalize()
{
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath)
{
	// 登録済みの名前かチェック
	if (particleGroups.find(name) != particleGroups.end())
	{
		Logger::Log("This ParticleGroup is already exist");
		assert(false);
	}

	// パーティクルグループを作成
	ParticleGroup newParticleGroup{};
	newParticleGroup.texture.texturePath = textureFilePath;
	newParticleGroup.texture.textureIndex = TextureManager::GetInstance()->GetSRVIndex(textureFilePath);

	// インスタンシング用リソースの生成
	newParticleGroup.particleDataForGPUResource_ = m_dx12_->MakeBufferResource(sizeof(ParticleDataForGPU) * kNumMaxInstance_);
	newParticleGroup.particleDataForGPUResource_->Map(0, nullptr, reinterpret_cast<void**>(&newParticleGroup.pParticleDataForGPU));

	for (uint32_t index = 0; index < kNumMaxInstance_; ++index)
	{
		newParticleGroup.pParticleDataForGPU[index].WVP = Mat4x4::MakeIdentity();
		newParticleGroup.pParticleDataForGPU[index].world = Mat4x4::MakeIdentity();
	}

	// インスタンシング用SRVの生成
	newParticleGroup.instancingSrvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVForStructuredBuffer(newParticleGroup.instancingSrvIndex, newParticleGroup.particleDataForGPUResource_.Get(), kNumMaxInstance_, sizeof(ParticleDataForGPU));

	// パーティクルグループをリストに追加
	particleGroups.emplace(name, newParticleGroup);
}

void ParticleManager::Emit(const std::string name, const Vector3& position, const Vector3& scale, uint32_t count)
{
	// パーティクルグループが存在するかをチェック
	if (particleGroups.find(name) == particleGroups.end())
	{
		Logger::Log("ParticleGroupが存在しません");
		assert(false);
	}

	// パーティクルグループを取得
	ParticleGroup& particleGroup = particleGroups[name];

	// 最大数に達している場合、スキップする
	if (particleGroup.particleList.size() >= count) {
		return;
	}

	// パーティクルの生成
	for (uint32_t index = 0; index < count; ++index)
	{
		// パーティクルの生成と追加
		particleGroup.particleList.push_back(MakeNewParticle(randomEngine_, position, scale));
	}
}

void ParticleManager::Emit(const std::string name, const Vector3& position, const Vector3& scale, uint32_t count, bool isRandomColor)
{
	if (particleGroups.find(name) == particleGroups.end())
	{
		Logger::Log("ParticleGroupが存在しません");
		assert(false);
	}

	ParticleGroup& particleGroup = particleGroups[name];

	if (particleGroup.particleList.size() >= count) {
		return;
	}

	for (uint32_t index = 0; index < count; ++index)
	{
		particleGroup.particleList.push_back(MakeNewParticle(randomEngine_, position, scale, isRandomColor));
	}
}

void ParticleManager::Emit(const std::string name, const Vector3& position, const Vector3& scale, uint32_t count, Vector4 color)
{
	if (particleGroups.find(name) == particleGroups.end())
	{
		Logger::Log("ParticleGroupが存在しません");
		assert(false);
	}

	ParticleGroup& particleGroup = particleGroups[name];

	if (particleGroup.particleList.size() >= count) {
		return;
	}

	for (uint32_t index = 0; index < count; ++index)
	{
		particleGroup.particleList.push_back(MakeNewParticle(randomEngine_, position, scale, color));
	}
}

ParticleManager::Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale)
{
	//一様分布生成器を使って乱数を生成
	std::uniform_real_distribution<float> random(-1.0f, 1.0f);
	std::uniform_real_distribution<float> randomColor(0.0f, 1.0f);
	std::uniform_real_distribution<float> randomTime(1.0f, 3.0f);

	Particle particle;

	// パーティクルの初期値を設定
	particle.transform.scale = scale;
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	Vector3 randomPosVec = { random(randomEngine), random(randomEngine), random(randomEngine) };
	particle.transform.translate = translate + randomPosVec;

	particle.velocity = { random(randomEngine), random(randomEngine), random(randomEngine) };
	particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	particle.lifeTime = randomTime(randomEngine);
	particle.currentTime = 0.0f;

	return particle;
}

ParticleManager::Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale, bool isRandomColor)
{
	Particle particle = MakeNewParticle(randomEngine, translate, scale);

	if (isRandomColor)
	{
		std::uniform_real_distribution<float> randomColor(0.0f, 1.0f);
		particle.color = { randomColor(randomEngine), randomColor(randomEngine), randomColor(randomEngine), 1.0f };
	}

	return particle;
}

ParticleManager::Particle ParticleManager::MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector3& scale, Vector4 color)
{
	Particle particle = MakeNewParticle(randomEngine, translate, scale);

	particle.color = color;

	return particle;

}

void ParticleManager::CreateRootSignature()
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
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_DESCRIPTOR_RANGE descriptorRangeForParticle[1] = {};
	descriptorRangeForParticle[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRangeForParticle[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRangeForParticle[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForParticle[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	// Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	// ParticleData
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForParticle; // ディスクリプタレンジを設定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForParticle); // レンジの数

	// Texture
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // ディスクリプタレンジを設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // レンジの数

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
	signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf());
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePSO()
{
	HRESULT hr;

	// RootSignatureの生成
	CreateRootSignature();

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {};
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

	inputElementDescs[3].SemanticName = "COLOR";
	inputElementDescs[3].SemanticIndex = 0;
	inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// shaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// depthの機能を有効化にする
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
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
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

void ParticleManager::CreateVertexData()
{
	modelData_.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData_.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData_.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData_.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData_.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });
	modelData_.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });

	// 頂点リソース生成
	vertexResource_ = m_dx12_->MakeBufferResource(sizeof(VertexData) * modelData_.vertices.size());

	// VertexBufferViewの作成
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();				// リソースの先頭のアドレスから使う
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());	// 使用するリソースのサイズは頂点のサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);									// 1頂点あたりのサイズ

	// 頂点リソースをマップ
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	// 頂点データをリソースにコピー
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
}

void ParticleManager::CreateMaterialData()
{
	// マテリアルリソース生成
	materialResource_ = m_dx12_->MakeBufferResource(sizeof(Material));

	// マテリアルリソースをマップ
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルデータの初期値を設定
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->uvTransform = Mat4x4::MakeIdentity();
}