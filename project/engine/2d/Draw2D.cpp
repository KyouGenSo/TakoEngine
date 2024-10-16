#include "Draw2D.h"
#include "Logger.h"
#include "imgui.h"

Draw2D* Draw2D::instance_ = nullptr;

Draw2D* Draw2D::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new Draw2D();
	}
	return instance_;
}

void Draw2D::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

	// パイプラインステートの生成
	CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, trianglePipelineState_);
	CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, linePipelineState_);

	// 座標変換行列データの生成
	CreateTransformMatData();
}

void Draw2D::Finalize()
{
	for (auto triangleData : triangleDatas_)
	{
		triangleData->vertexBuffer->Unmap(0, nullptr);
		triangleData->colorBuffer->Unmap(0, nullptr);

		triangleData->vertexBuffer->Release();
		triangleData->colorBuffer->Release();

	}

	triangleDatas_.clear();

	for (auto lineData : lineDatas_)
	{
		lineData->vertexBuffer->Unmap(0, nullptr);
		lineData->colorBuffer->Unmap(0, nullptr);

		lineData->vertexBuffer->Release();
		lineData->colorBuffer->Release();
	}

	lineDatas_.clear();

	transformationMatrixBuffer_->Unmap(0, nullptr);
	transformationMatrixBuffer_->Release();

	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Draw2D::Update()
{
	triangleDatas_.clear();
	lineDatas_.clear();
}

void Draw2D::ImGui()
{
#ifdef _DEBUG
	ImGui::Begin("Draw2D");

	// triangleDatas_の要素数を表示
	ImGui::Text("TriangleData Count : %d", triangleDatas_.size());

	ImGui::End();
#endif // _DEBUG
}

void Draw2D::DrawTriangle(const Vector2& pos1, const Vector2& pos2, const Vector2& pos3, const Vector4& color)
{
	TriangleData* triangleData = new TriangleData();

	// 三角形の頂点データを生成
	InitializeTriangleData(triangleData);

	// 頂点データの設定
	triangleData->vertexData[0].position = Vector2(pos1.x, pos1.y);
	triangleData->vertexData[1].position = Vector2(pos2.x, pos2.y);
	triangleData->vertexData[2].position = Vector2(pos3.x, pos3.y);

	// カラーデータの設定
	triangleData->color[0] = color;

	// 共通の描画設定
	SetCommonRenderSetting();

	// 頂点バッファビューの設定
	m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &triangleData->vertexBufferView);

	// 座標変換行列の設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixBuffer_->GetGPUVirtualAddress());

	// カラーバッファの設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, triangleData->colorBuffer->GetGPUVirtualAddress());

	// 描画
	m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);

	// 三角形データを保存
	if (triangleData != nullptr)
	{
		triangleDatas_.push_back(triangleData);
	}
}

void Draw2D::DrawLine(const Vector2& start, const Vector2& end, const Vector4& color)
{
	LineData* lineData = new LineData();

	// 線の頂点データを生成
	InitializeLineData(lineData);

	// 頂点データの設定
	lineData->vertexData[0].position = Vector2(start.x, start.y);
	lineData->vertexData[1].position = Vector2(end.x, end.y);

	// カラーデータの設定
	lineData->color[0] = color;

	// ルートシグネチャの設定
	m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// パイプラインステートの設定
	m_dx12_->GetCommandList()->SetPipelineState(linePipelineState_.Get());

	// トポロジの設定
	m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// 頂点バッファビューの設定
	m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &lineData->vertexBufferView);

	// 座標変換行列の設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixBuffer_->GetGPUVirtualAddress());

	// カラーバッファの設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, lineData->colorBuffer->GetGPUVirtualAddress());

	// 描画
	m_dx12_->GetCommandList()->DrawInstanced(2, 1, 0, 0);

	// 線データを保存
	if (lineData != nullptr)
	{
		lineDatas_.push_back(lineData);
	}
}

void Draw2D::SetCommonRenderSetting()
{
	// ルートシグネチャの設定
	m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// パイプラインステートの設定
	m_dx12_->GetCommandList()->SetPipelineState(trianglePipelineState_.Get());

	// トポロジの設定
	m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Draw2D::CreateRootSignature()
{
	HRESULT hr;

	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[2] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド 

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

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

void Draw2D::CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12PipelineState>& pipelineState)
{

	HRESULT hr;

	// RootSignatureの生成
	CreateRootSignature();

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// shaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/2D.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/2D.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

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
	graphicsPipelineStateDesc.PrimitiveTopologyType = primitiveTopologyType;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// 実際に生成
	hr = m_dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(hr));
}

void Draw2D::CreateTriangleVertexData(TriangleData* triangleData)
{
	// 頂点リソースを生成
	triangleData->vertexBuffer = m_dx12_->MakeBufferResource(sizeof(VertexData) * 3);

	// 頂点バッファビューを作成する
	triangleData->vertexBufferView.BufferLocation = triangleData->vertexBuffer->GetGPUVirtualAddress();
	triangleData->vertexBufferView.SizeInBytes = sizeof(VertexData) * 3;
	triangleData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	triangleData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&triangleData->vertexData));

}

void Draw2D::CreateLineVertexData(LineData* lineData)
{
	// 頂点リソースを生成
	lineData->vertexBuffer = m_dx12_->MakeBufferResource(sizeof(VertexData) * 2);

	// 頂点バッファビューを作成する
	lineData->vertexBufferView.BufferLocation = lineData->vertexBuffer->GetGPUVirtualAddress();
	lineData->vertexBufferView.SizeInBytes = sizeof(VertexData) * 2;
	lineData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	lineData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lineData->vertexData));
}

void Draw2D::CreateColorData(TriangleData* triangleData)
{
	// カラーリソースを生成
	triangleData->colorBuffer = m_dx12_->MakeBufferResource(sizeof(Vector4));

	// カラーリソースをマップ
	triangleData->colorBuffer->Map(0, nullptr, reinterpret_cast<void**>(&triangleData->color));
}

void Draw2D::CreateColorData(LineData* lineData)
{
	// カラーリソースを生成
	lineData->colorBuffer = m_dx12_->MakeBufferResource(sizeof(Vector4));

	// カラーリソースをマップ
	lineData->colorBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lineData->color));
}

void Draw2D::CreateTransformMatData()
{
	// 座標変換行列リソースを生成
	transformationMatrixBuffer_ = m_dx12_->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	// 座標変換行列データの初期値を書き込む
	Matrix4x4 worldMatrix = Mat4x4::MakeIdentity();
	Matrix4x4 viewMatrix = Mat4x4::MakeIdentity();
	Matrix4x4 projectionMatrix = Mat4x4::MakeOrtho(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	Matrix4x4 wvpMatrix = Mat4x4::Multiply(worldMatrix, Mat4x4::Multiply(viewMatrix, projectionMatrix));

	transformationMatrixData_->WVP = wvpMatrix;
	transformationMatrixData_->world = worldMatrix;
}

void Draw2D::InitializeTriangleData(TriangleData* triangleData)
{
	CreateTriangleVertexData(triangleData);

	CreateColorData(triangleData);
}

void Draw2D::InitializeLineData(LineData* lineData)
{
	CreateLineVertexData(lineData);

	CreateColorData(lineData);
}



