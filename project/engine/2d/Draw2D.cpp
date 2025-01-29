#include "Draw2D.h"
#include "Logger.h"
#include "imgui.h"
#include "DebugCamera.h"
#include <cassert>

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

	isDebug_ = false;

	projectionMatrix_ = Mat4x4::MakeOrtho(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 1.0f);
	viewPortMatrix_ = Mat4x4::MakeViewport(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 1.0f);

	// パイプラインステートの生成
	CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, trianglePipelineState_, triangleRootSignature_);
	CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, linePipelineState_, lineRootSignature_);

	// 座標変換行列データの生成
	CreateTransformMatData();

	// 三角形の頂点データを生成
	triangleData_ = new TriangleData();
	CreateTriangleVertexData(triangleData_);

	// 矩形の頂点データを生成
	boxData_ = new BoxData();
	CreateBoxVertexData(boxData_);

	// 線の頂点データを生成
	lineData_ = new LineData();
	CreateLineVertexData(lineData_);

	// 球の頂点位置を計算
	CalcSphereVertexData();
}

void Draw2D::Finalize()
{
	transformationMatrixBuffer_->Release();

	triangleData_->vertexBuffer->Release();

	boxData_->vertexBuffer->Release();

	boxData_->indexBuffer->Release();

	lineData_->vertexBuffer->Release();


	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Draw2D::Update()
{
	if (!isDebug_) {
		transformationMatrixData_->WVP = m_camera_->GetViewMatrix() * m_camera_->GetProjectionMatrix();
	} else {
		transformationMatrixData_->WVP = DebugCamera::GetInstance()->GetViewProjectionMat();
	}
}

void Draw2D::ImGui()
{
#ifdef _DEBUG

#endif // _DEBUG
}

void Draw2D::DrawTriangle(const Vector3& pos1, const Vector3& pos2, const Vector3& pos3, const Vector4& color)
{

	// 頂点データの設定
	triangleData_->vertexData[triangleIndex_].position = pos1;
	triangleData_->vertexData[triangleIndex_ + 1].position = pos2;
	triangleData_->vertexData[triangleIndex_ + 2].position = pos3;

	// カラーデータの設定
	triangleData_->vertexData[triangleIndex_].color = color;
	triangleData_->vertexData[triangleIndex_ + 1].color = color;
	triangleData_->vertexData[triangleIndex_ + 2].color = color;


	triangleIndex_ += kVertexCountTrriangle;

}

void Draw2D::DrawBox(const Vector3& pos, const Vector3& size, const Vector4& color)
{
	// 頂点データの設定
	boxData_->vertexData[boxVertexIndex_].position = Vector3(pos.x, pos.y, pos.z);
	boxData_->vertexData[boxVertexIndex_ + 1].position = Vector3(pos.x + size.x, pos.y, pos.z);
	boxData_->vertexData[boxVertexIndex_ + 2].position = Vector3(pos.x + size.x, pos.y + size.y, pos.z);
	boxData_->vertexData[boxVertexIndex_ + 3].position = Vector3(pos.x, pos.y + size.y, pos.z);

	// インデックスデータの設定
	boxData_->indexData[boxIndexIndex_] = 0;
	boxData_->indexData[boxIndexIndex_ + 1] = 1;
	boxData_->indexData[boxIndexIndex_ + 2] = 2;
	boxData_->indexData[boxIndexIndex_ + 3] = 0;
	boxData_->indexData[boxIndexIndex_ + 4] = 2;
	boxData_->indexData[boxIndexIndex_ + 5] = 3;

	// カラーデータの設定
	boxData_->vertexData[boxVertexIndex_].color = color;
	boxData_->vertexData[boxVertexIndex_ + 1].color = color;
	boxData_->vertexData[boxVertexIndex_ + 2].color = color;
	boxData_->vertexData[boxVertexIndex_ + 3].color = color;

	boxIndexIndex_ += kIndexCountBox;
	boxVertexIndex_ += kVertexCountBox;
}

void Draw2D::DrawBox(const Vector3& pos, const Vector3& size, const float angle, const Vector4& color)
{
	// 回転行列の生成
	Matrix4x4 rotationMatrix = Mat4x4::MakeRotateZ(angle);

	float left = 0.0f;
	float right = size.x;
	float top = 0.0f;
	float bottom = size.y;

	std::array<Vector2, 4> vertexPos =
	{
		Vector2(left, top),    // 左上
		Vector2(right, top),   // 右上
		Vector2(right, bottom),// 右下
		Vector2(left, bottom), // 左下
	};

	// 回転
	for (auto& vertex : vertexPos)
	{
		Vector3 pos2D = { vertex.x, vertex.y, 0.0f };
		pos2D = Mat4x4::TransForm(rotationMatrix, Vector3(pos2D.x, pos2D.y, 0.0f));
		vertex = Vector2(pos2D.x, pos2D.y);
	}

	// 頂点データの設定
	boxData_->vertexData[boxVertexIndex_].position = Vector3(pos.x + vertexPos[0].x, pos.y + vertexPos[0].y, pos.z);
	boxData_->vertexData[boxVertexIndex_ + 1].position = Vector3(pos.x + vertexPos[1].x, pos.y + vertexPos[1].y, pos.z);
	boxData_->vertexData[boxVertexIndex_ + 2].position = Vector3(pos.x + vertexPos[2].x, pos.y + vertexPos[2].y, pos.z);
	boxData_->vertexData[boxVertexIndex_ + 3].position = Vector3(pos.x + vertexPos[3].x, pos.y + vertexPos[3].y, pos.z);

	// インデックスデータの設定
	boxData_->indexData[boxIndexIndex_] = 0;
	boxData_->indexData[boxIndexIndex_ + 1] = 1;
	boxData_->indexData[boxIndexIndex_ + 2] = 2;
	boxData_->indexData[boxIndexIndex_ + 3] = 0;
	boxData_->indexData[boxIndexIndex_ + 4] = 2;
	boxData_->indexData[boxIndexIndex_ + 5] = 3;

	// カラーデータの設定
	boxData_->vertexData[boxVertexIndex_].color = color;
	boxData_->vertexData[boxVertexIndex_ + 1].color = color;
	boxData_->vertexData[boxVertexIndex_ + 2].color = color;
	boxData_->vertexData[boxVertexIndex_ + 3].color = color;

	boxIndexIndex_ += kIndexCountBox;
	boxVertexIndex_ += kVertexCountBox;
}

void Draw2D::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color)
{

	// 頂点データの設定
	lineData_->vertexData[lineIndex_].position = start;
	lineData_->vertexData[lineIndex_ + 1].position = end;

	// カラーデータの設定
	lineData_->vertexData[lineIndex_].color = color;
	lineData_->vertexData[lineIndex_ + 1].color = color;

	lineIndex_ += kVertexCountLine;

}

void Draw2D::DrawSphere(const Vector3& center, const float radius, const Vector4& color)
{
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(Vector3(radius, radius, radius), Vector3(0.0f, 0.0f, 0.0f), center);

	for (uint32_t i = 0; i < sphereVerties_.size(); i += 3)
	{
		Vector3 a = sphereVerties_[i];
		Vector3 b = sphereVerties_[i + 1];
		Vector3 c = sphereVerties_[i + 2];

		a = Mat4x4::TransForm(worldMatrix, a);
		b = Mat4x4::TransForm(worldMatrix, b);
		c = Mat4x4::TransForm(worldMatrix, c);

		// ライン描画
		DrawLine(a, b, color);
		DrawLine(b, c, color);
	}
}

void Draw2D::DrawAABB(const AABB& aabb, const Vector4& color)
{
	Vector3 min = aabb.min;
	Vector3 max = aabb.max;

	Vector3 p1 = Vector3(min.x, min.y, min.z);
	Vector3 p2 = Vector3(max.x, min.y, min.z);
	Vector3 p3 = Vector3(max.x, max.y, min.z);
	Vector3 p4 = Vector3(min.x, max.y, min.z);
	Vector3 p5 = Vector3(min.x, min.y, max.z);
	Vector3 p6 = Vector3(max.x, min.y, max.z);
	Vector3 p7 = Vector3(max.x, max.y, max.z);
	Vector3 p8 = Vector3(min.x, max.y, max.z);

	// 底面
	DrawLine(p1, p2, color);
	DrawLine(p2, p3, color);
	DrawLine(p3, p4, color);
	DrawLine(p4, p1, color);

	// 上面
	DrawLine(p5, p6, color);
	DrawLine(p6, p7, color);
	DrawLine(p7, p8, color);
	DrawLine(p8, p5, color);

	// 側面
	DrawLine(p1, p5, color);
	DrawLine(p2, p6, color);
	DrawLine(p3, p7, color);
	DrawLine(p4, p8, color);
}

void Draw2D::DrawGrid(const float size, const float subdivision, const Vector4& color)
{
	float halfWidth = size * 0.5f;
	float every = size / subdivision;

	for (uint32_t xIndex = 0; xIndex <= subdivision; xIndex++)
	{
		Vector3 worldStart = Vector3(-halfWidth + every * xIndex, 0.0f, halfWidth);
		Vector3 worldEnd = Vector3(-halfWidth + every * xIndex, 0.0f, -halfWidth);

		DrawLine(worldStart, worldEnd, color);
	}

	for (uint32_t zIndex = 0; zIndex <= subdivision; zIndex++)
	{
		Vector3 worldStart = Vector3(halfWidth, 0.0f, -halfWidth + every * zIndex);
		Vector3 worldEnd = Vector3(-halfWidth, 0.0f, -halfWidth + every * zIndex);

		DrawLine(worldStart, worldEnd, color);
	}

	// X軸
	DrawLine(Vector3(-halfWidth, 0.0f, 0.0f), Vector3(halfWidth, 0.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f));
	// Z軸
	DrawLine(Vector3(0.0f, 0.0f, -halfWidth), Vector3(0.0f, 0.0f, halfWidth), Vector4(0.0f, 0.0f, 1.0f, 1.0f));
	// Y軸
	DrawLine(Vector3(0.0f, -halfWidth, 0.0f), Vector3(0.0f, halfWidth, 0.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f));

}

void Draw2D::Draw()
{
	/// ================================== ///
	///              線の描画               ///
	/// ================================== ///
	// ルートシグネチャの設定
	m_dx12_->GetCommandList()->SetGraphicsRootSignature(lineRootSignature_.Get());

	// パイプラインステートの設定
	m_dx12_->GetCommandList()->SetPipelineState(linePipelineState_.Get());

	// トポロジの設定
	m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// 頂点バッファビューの設定
	m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &lineData_->vertexBufferView);

	// 座標変換行列の設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());

	// 描画
	m_dx12_->GetCommandList()->DrawInstanced(lineIndex_, lineIndex_ / kVertexCountLine, 0, 0);


	/// ================================== ///
	///              三角形の描画            ///
	/// ================================== ///
	// ルートシグネチャの設定
	m_dx12_->GetCommandList()->SetGraphicsRootSignature(triangleRootSignature_.Get());

	// パイプラインステートの設定
	m_dx12_->GetCommandList()->SetPipelineState(trianglePipelineState_.Get());

	// トポロジの設定
	m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 頂点バッファビューの設定
	m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &triangleData_->vertexBufferView);

	// 座標変換行列の設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());

	// 描画
	m_dx12_->GetCommandList()->DrawInstanced(triangleIndex_, triangleIndex_ / kVertexCountTrriangle, 0, 0);

	/// ================================== ///
	///              BOXの描画              ///
	/// ================================== ///
	// 頂点バッファビューの設定
	m_dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &boxData_->vertexBufferView);

	// インデックスバッファビューの設定
	m_dx12_->GetCommandList()->IASetIndexBuffer(&boxData_->indexBufferView);

	// 座標変換行列の設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());

	// 描画
	m_dx12_->GetCommandList()->DrawIndexedInstanced(kIndexCountBox, boxVertexIndex_ / kVertexCountBox, 0, 0, 0);
}

void Draw2D::Reset()
{
	triangleIndex_ = 0;
	boxVertexIndex_ = 0;
	boxIndexIndex_ = 0;
	lineIndex_ = 0;
}

void Draw2D::CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature)
{
	HRESULT hr;

	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[1] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

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

	hr = m_dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
	signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf());
	assert(SUCCEEDED(hr));
}

void Draw2D::CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSignature)
{

	HRESULT hr;

	// RootSignatureの生成
	CreateRootSignature(rootSignature);

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "COLOR";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

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
	// 裏面
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// shaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = m_dx12_->CompileShader(L"resources/shaders/2D.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = m_dx12_->CompileShader(L"resources/shaders/2D.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = triangleRootSignature_.Get();
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
	UINT vertexBufferSize = sizeof(VertexData) * kVertexCountTrriangle * kTrriangleMaxCount;

	// 頂点リソースを生成
	triangleData->vertexBuffer = m_dx12_->MakeBufferResource(vertexBufferSize);
	//m_dx12_->CreateBufferResource(triangleData->vertexBuffer, vertexBufferSize);

	// 頂点バッファビューを作成する
	triangleData->vertexBufferView.BufferLocation = triangleData->vertexBuffer->GetGPUVirtualAddress();
	triangleData->vertexBufferView.SizeInBytes = vertexBufferSize;
	triangleData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	triangleData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&triangleData->vertexData));

}

void Draw2D::CreateBoxVertexData(BoxData* boxData)
{
	UINT vertexBufferSize = sizeof(VertexData) * kVertexCountBox * kBoxMaxCount;
	UINT indexBufferSize = sizeof(uint32_t) * kIndexCountBox * kBoxMaxCount;

	// 頂点リソースを生成
	boxData->vertexBuffer = m_dx12_->MakeBufferResource(vertexBufferSize);

	// インデックスリソースを生成
	boxData->indexBuffer = m_dx12_->MakeBufferResource(indexBufferSize);

	// 頂点バッファビューを作成する
	boxData->vertexBufferView.BufferLocation = boxData->vertexBuffer->GetGPUVirtualAddress();
	boxData->vertexBufferView.SizeInBytes = vertexBufferSize;
	boxData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// インデックスバッファビューを作成する
	boxData->indexBufferView.BufferLocation = boxData->indexBuffer->GetGPUVirtualAddress();
	boxData->indexBufferView.SizeInBytes = indexBufferSize;
	boxData->indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// 頂点リソースをマップ
	boxData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxData->vertexData));

	// インデックスリソースをマップ
	boxData->indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&boxData->indexData));
}

void Draw2D::CreateLineVertexData(LineData* lineData)
{
	UINT vertexBufferSize = sizeof(VertexData) * kVertexCountLine * kLineMaxCount;

	// 頂点リソースを生成
	lineData->vertexBuffer = m_dx12_->MakeBufferResource(vertexBufferSize);
	//m_dx12_->CreateBufferResource(lineData->vertexBuffer, vertexBufferSize);

	// 頂点バッファビューを作成する
	lineData->vertexBufferView.BufferLocation = lineData->vertexBuffer->GetGPUVirtualAddress();
	lineData->vertexBufferView.SizeInBytes = vertexBufferSize;
	lineData->vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースをマップ
	lineData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lineData->vertexData));
}

void Draw2D::CreateTransformMatData()
{
	// 座標変換行列リソースを生成
	transformationMatrixBuffer_ = m_dx12_->MakeBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースをマップ
	transformationMatrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = m_camera_->GetViewMatrix() * m_camera_->GetProjectionMatrix();
}

void Draw2D::CalcSphereVertexData()
{
	const uint32_t kSubdivision = 4; // 1分割数
	const float kLonEvery = 2.0f * 3.14159265359f / float(kSubdivision); // 経度の1分割の角度 phi
	const float kLatEvery = 3.14159265359f / float(kSubdivision); // 緯度の1分割の角度 theta

	// 緯度方向のループ
	for (uint32_t latIndex = 0; latIndex < kSubdivision; latIndex++) {
		float lat = -3.14159265359f / 2.0f + kLatEvery * float(latIndex);
		// 経度方向のループ
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; lonIndex++) {
			float lon = kLonEvery * float(lonIndex);
			// 球の表面上の点を求める
			Vector3 a, b, c;
			a.x = 0.0f + 1.0f * cosf(lat) * cosf(lon);
			a.y = 0.0f + 1.0f * sinf(lat);
			a.z = 0.0f + 1.0f * cosf(lat) * sinf(lon);
			b.x = 0.0f + 1.0f * cosf(lat + kLatEvery) * cosf(lon);
			b.y = 0.0f + 1.0f * sinf(lat + kLatEvery);
			b.z = 0.0f + 1.0f * cosf(lat + kLatEvery) * sinf(lon);
			c.x = 0.0f + 1.0f * cosf(lat) * cosf(lon + kLonEvery);
			c.y = 0.0f + 1.0f * sinf(lat);
			c.z = 0.0f + 1.0f * cosf(lat) * sinf(lon + kLonEvery);

			// 座標を保存
			sphereVerties_.push_back(a);
			sphereVerties_.push_back(b);
			sphereVerties_.push_back(c);
		}
	}
}

void Draw2D::CalcGridVertexData()
{
	const float kGridHalfWidth = 2.0f; // グリッドの半分の幅
	const uint32_t kSubdivision = 10; // 1分割数
	const float kGridEvery = (kGridHalfWidth * 2.0f) / float(kSubdivision); // 1分割の長さ

	// 奥から手前への線を順々に引いていく
	for (uint32_t xIndex = 0; xIndex <= kSubdivision; xIndex++) {
		// 上の情報を使ってワールド座標系上の始点と終点を求める
		Vector3 worldStart(-kGridHalfWidth + kGridEvery * float(xIndex), 0.0f, kGridHalfWidth);
		Vector3 worldEnd(-kGridHalfWidth + kGridEvery * float(xIndex), 0.0f, -kGridHalfWidth);

		// 座標を保存
		gridVerties_.push_back(worldStart);
		gridVerties_.push_back(worldEnd);
	}

	// 左から右への線を順々に引いていく
	for (uint32_t zIndex = 0; zIndex <= kSubdivision; zIndex++) {
		// 上の情報を使ってワールド座標系上の始点と終点を求める
		Vector3 worldStart(-kGridHalfWidth, 0.0f, kGridHalfWidth - kGridEvery * float(zIndex));
		Vector3 worldEnd(kGridHalfWidth, 0.0f, kGridHalfWidth - kGridEvery * float(zIndex));

		// 座標を保存
		gridVerties_.push_back(worldStart);
		gridVerties_.push_back(worldEnd);
	}
}




