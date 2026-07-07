#include "Draw2D.h"
#include "OBB.h"
#include "EnginePaths.h"
#include <cassert>
#include <cmath>
#include <numbers>

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "DebugCamera.h"
#endif // DEBUG

namespace Tako {

  std::unique_ptr<Draw2D> Draw2D::instance_ = nullptr;

  Draw2D* Draw2D::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<Draw2D>(new Draw2D());
    }
    return instance_.get();
  }

  void Draw2D::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    isDebug_ = false;

    projectionMatrix_ = Mat4x4::MakeOrtho(0.0f, 0.0f, static_cast<float>(WinApp::clientWidth), static_cast<float>(WinApp::clientHeight), 0.0f, 1.0f);

    // パイプラインステートの生成
    CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, trianglePipelineState_, triangleRootSignature_);
    CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, linePipelineState_, lineRootSignature_);

    // 座標変換行列データの生成
    CreateTransformMatData();

    // 三角形の頂点データを生成
    triangleData_ = std::make_unique<TriangleData>();
    CreateTriangleVertexData(triangleData_.get());

    // 矩形の頂点データを生成
    boxData_ = std::make_unique<BoxData>();
    CreateBoxVertexData(boxData_.get());

    // 線の頂点データを生成
    lineData_ = std::make_unique<LineData>();
    CreateLineVertexData(lineData_.get());

  }

  void Draw2D::Finalize()
  {
    // unique_ptr がデストラクト時に ComPtr も自動解放するので
    // reset()だけで完全にクリーンアップされる
    triangleData_.reset();
    boxData_.reset();
    lineData_.reset();

    instance_.reset();
  }

  void Draw2D::Update()
  {
    if (!isDebug_) {
      transformationMatrixData_->WVP = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
    }
    else {
#ifdef _DEBUG
      transformationMatrixData_->WVP = DebugCamera::GetInstance()->GetViewProjectionMat();
#endif
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


    triangleIndex_ += kVertexCountTriangle;

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
    for (auto& vertex : vertexPos) {
      Vector3 pos2D = { vertex.x, vertex.y, 0.0f };
      pos2D = Mat4x4::Transform(rotationMatrix, Vector3(pos2D.x, pos2D.y, 0.0f));
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

  void Draw2D::DrawArrow(const Vector3& start, const Vector3& end, const Vector4& color, float headSize)
  {
    // 本体の線を描画
    DrawLine(start, end, color);

    // 矢印の方向と長さ
    Vector3 diff = end - start;
    float length = diff.Length();
    if (length < 0.001f) return;

    Vector3 dir = diff.Normalize();

    // dir に垂直な2つの軸を算出
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    if (std::abs(dir.Dot(up)) > 0.99f) {
      up = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = dir.Cross(up).Normalize();
    Vector3 upPerp = right.Cross(dir);

    // 矢印の先端4本の線（十字形状、3Dでどの角度からも矢印に見える）
    Vector3 headBase = end - dir * headSize;
    float halfHead = headSize * 0.5f;

    DrawLine(end, headBase + right * halfHead, color);
    DrawLine(end, headBase - right * halfHead, color);
    DrawLine(end, headBase + upPerp * halfHead, color);
    DrawLine(end, headBase - upPerp * halfHead, color);
  }

  void Draw2D::DrawSphere(const Vector3& center, const float radius, const Vector4& color, uint32_t subdivision)
  {
    const float kLonEvery = 2.0f * std::numbers::pi_v<float> / static_cast<float>(subdivision);
    const float kLatEvery = std::numbers::pi_v<float> / static_cast<float>(subdivision);

    for (uint32_t latIndex = 0; latIndex < subdivision; latIndex++) {
      float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * static_cast<float>(latIndex);
      for (uint32_t lonIndex = 0; lonIndex < subdivision; lonIndex++) {
        float lon = kLonEvery * static_cast<float>(lonIndex);

        // 単位球上の3点を計算し、radius + center でワールド座標に変換
        Vector3 a = {
          cosf(lat) * cosf(lon) * radius + center.x,
          sinf(lat) * radius + center.y,
          cosf(lat) * sinf(lon) * radius + center.z
        };
        Vector3 b = {
          cosf(lat + kLatEvery) * cosf(lon) * radius + center.x,
          sinf(lat + kLatEvery) * radius + center.y,
          cosf(lat + kLatEvery) * sinf(lon) * radius + center.z
        };
        Vector3 c = {
          cosf(lat) * cosf(lon + kLonEvery) * radius + center.x,
          sinf(lat) * radius + center.y,
          cosf(lat) * sinf(lon + kLonEvery) * radius + center.z
        };

        DrawLine(a, b, color);
        DrawLine(b, c, color);
      }
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

  void Draw2D::DrawOBB(const OBB& obb, const Vector4& color)
  {
    // OBB の8つの頂点を取得
    std::array<Vector3, 8> vertices = obb.GetVertices();

    // 底面（頂点0,1,2,3）
    DrawLine(vertices[0], vertices[1], color);
    DrawLine(vertices[1], vertices[2], color);
    DrawLine(vertices[2], vertices[3], color);
    DrawLine(vertices[3], vertices[0], color);

    // 上面（頂点4,5,6,7）
    DrawLine(vertices[4], vertices[5], color);
    DrawLine(vertices[5], vertices[6], color);
    DrawLine(vertices[6], vertices[7], color);
    DrawLine(vertices[7], vertices[4], color);

    // 側面（底面と上面をつなぐ）
    DrawLine(vertices[0], vertices[4], color);
    DrawLine(vertices[1], vertices[5], color);
    DrawLine(vertices[2], vertices[6], color);
    DrawLine(vertices[3], vertices[7], color);
  }

  void Draw2D::DrawGrid(const float size, const float subdivision, const Vector4& color)
  {
    float halfWidth = size * 0.5f;
    float every = size / subdivision;

    for (uint32_t xIndex = 0; xIndex <= subdivision; xIndex++) {
      Vector3 worldStart = Vector3(-halfWidth + every * xIndex, 0.0f, halfWidth);
      Vector3 worldEnd = Vector3(-halfWidth + every * xIndex, 0.0f, -halfWidth);

      DrawLine(worldStart, worldEnd, color);
    }

    for (uint32_t zIndex = 0; zIndex <= subdivision; zIndex++) {
      Vector3 worldStart = Vector3(halfWidth, 0.0f, -halfWidth + every * zIndex);
      Vector3 worldEnd = Vector3(-halfWidth, 0.0f, -halfWidth + every * zIndex);

      DrawLine(worldStart, worldEnd, color);
    }

    // X 軸
    DrawLine(Vector3(-halfWidth, 0.0f, 0.0f), Vector3(halfWidth, 0.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f));
    // Z 軸
    DrawLine(Vector3(0.0f, 0.0f, -halfWidth), Vector3(0.0f, 0.0f, halfWidth), Vector4(0.0f, 0.0f, 1.0f, 1.0f));
    // Y 軸
    DrawLine(Vector3(0.0f, -halfWidth, 0.0f), Vector3(0.0f, halfWidth, 0.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f));

  }

  void Draw2D::Draw()
  {
    /// ================================== ///
    ///              線の描画               ///
    /// ================================== ///
    // 描画する線がある場合のみ処理
    if (lineIndex_ > 0) {
      // ルートシグネチャの設定
      dx12_->GetCommandList()->SetGraphicsRootSignature(lineRootSignature_.Get());

      // パイプラインステートの設定
      dx12_->GetCommandList()->SetPipelineState(linePipelineState_.Get());

      // トポロジの設定
      dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

      // 頂点バッファビューの設定
      dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &lineData_->vertexBufferView);

      // 座標変換行列の設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());

      // 描画
      dx12_->GetCommandList()->DrawInstanced(lineIndex_, lineIndex_ / kVertexCountLine, 0, 0);
    }


    /// ================================== ///
    ///              三角形の描画            ///
    /// ================================== ///
    // 描画する三角形がある場合のみ処理
    if (triangleIndex_ > 0) {
      // ルートシグネチャの設定
      dx12_->GetCommandList()->SetGraphicsRootSignature(triangleRootSignature_.Get());

      // パイプラインステートの設定
      dx12_->GetCommandList()->SetPipelineState(trianglePipelineState_.Get());

      // トポロジの設定
      dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      // 頂点バッファビューの設定
      dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &triangleData_->vertexBufferView);

      // 座標変換行列の設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());

      // 描画
      dx12_->GetCommandList()->DrawInstanced(triangleIndex_, triangleIndex_ / kVertexCountTriangle, 0, 0);
    }

    /// ================================== ///
    ///              BOX の描画              ///
    /// ================================== ///
    // 描画する BOX がある場合のみ処理
    if (boxVertexIndex_ > 0) {
      // 頂点バッファビューの設定
      dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &boxData_->vertexBufferView);

      // インデックスバッファビューの設定
      dx12_->GetCommandList()->IASetIndexBuffer(&boxData_->indexBufferView);

      // 座標変換行列の設定
      dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());

      // 描画
      dx12_->GetCommandList()->DrawIndexedInstanced(kIndexCountBox, boxVertexIndex_ / kVertexCountBox, 0, 0, 0);
    }
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

    // rootSignature の生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter の設定。複数設定できるので配列
    D3D12_ROOT_PARAMETER rootParameters[1] = {};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
    rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        reinterpret_cast<char*>(errorBlob->GetBufferPointer()),
        DebugUIManager::LogType::Error);
#endif

      assert(false);
    }

    hr = dx12_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
    assert(SUCCEEDED(hr));
  }

  void Draw2D::CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12RootSignature>& rootSignature)
  {

    HRESULT hr;

    // RootSignature の生成
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

    // shader のコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"2D.VS.hlsl"), L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx12_->CompileShader(EnginePaths::ShaderPath(L"2D.PS.hlsl"), L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // PSO の生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = triangleRootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    // 書き込む RTV の情報
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // 利用するトポロジ（形状）のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType = primitiveTopologyType;
    // どのように画面に色を打ち込むかの設定
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 実際に生成
    hr = dx12_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(hr));
  }

  void Draw2D::CreateTriangleVertexData(TriangleData* triangleData)
  {
    UINT vertexBufferSize = sizeof(VertexData) * kVertexCountTriangle * kTriangleMaxCount;

    // 頂点リソースを生成
    triangleData->vertexBuffer = dx12_->MakeBufferResource(vertexBufferSize);
    //dx12_->CreateBufferResource(triangleData->vertexBuffer, vertexBufferSize);

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
    boxData->vertexBuffer = dx12_->MakeBufferResource(vertexBufferSize);

    // インデックスリソースを生成
    boxData->indexBuffer = dx12_->MakeBufferResource(indexBufferSize);

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
    lineData->vertexBuffer = dx12_->MakeBufferResource(vertexBufferSize);
    //dx12_->CreateBufferResource(lineData->vertexBuffer, vertexBufferSize);

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
    transformationMatrixBuffer_ = dx12_->MakeBufferResource(sizeof(TransformationMatrix));

    // 座標変換行列リソースをマップ
    transformationMatrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

    transformationMatrixData_->WVP = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
  }

} // namespace Tako




