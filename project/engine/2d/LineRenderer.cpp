#include "LineRenderer.h"
#include "OBB.h"
#include <cassert>
#include <cmath>
#include <numbers>

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif // DEBUG

namespace Tako {

  std::unique_ptr<LineRenderer> LineRenderer::instance_ = nullptr;

  LineRenderer* LineRenderer::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<LineRenderer>(Token{});
    }
    return instance_.get();
  }

  void LineRenderer::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    CreatePipelines();

    // 線の頂点データを生成
    lineData_ = std::make_unique<LineData>();
    CreateLineVertexData(lineData_.get());

#ifdef _DEBUG
    // エディタプレビュー用の第2バッファ（メインバッチと同一フレームで別視点を併存させるため分離）
    previewLineData_ = std::make_unique<LineData>();
    CreateLineVertexData(previewLineData_.get(), kPreviewLineMaxCount);
#endif // _DEBUG
  }

  void LineRenderer::Finalize()
  {
    // unique_ptr がデストラクト時に ComPtr も自動解放するので
    // reset()だけで完全にクリーンアップされる
    lineData_.reset();
#ifdef _DEBUG
    previewLineData_.reset();
#endif // _DEBUG

    instance_.reset();
  }

  void LineRenderer::Update()
  {
#ifdef _DEBUG
    previewLineIndex_ = 0;
    previewDrawStart_ = 0;
#endif // _DEBUG
  }

  void LineRenderer::ImGui()
  {
#ifdef _DEBUG

#endif // _DEBUG
  }

  void LineRenderer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color)
  {
#ifdef _DEBUG
    // プレビューバッチ中はプレビュー専用バッファへ振り向ける
    if (previewBatchMode_) {
      if (previewLineIndex_ + kVertexCountLine > kPreviewLineMaxCount * kVertexCountLine) {
        return;
      }
      previewLineData_->vertexData[previewLineIndex_].position = start;
      previewLineData_->vertexData[previewLineIndex_ + 1].position = end;
      previewLineData_->vertexData[previewLineIndex_].color = color;
      previewLineData_->vertexData[previewLineIndex_ + 1].color = color;
      previewLineIndex_ += kVertexCountLine;
      return;
    }
#endif // _DEBUG

    // 上限超過分は破棄する（Map 済み領域外への書き込み防止）
    if (lineIndex_ + kVertexCountLine > kLineMaxCount * kVertexCountLine) {
#ifdef _DEBUG
      static bool overflowWarned = false;
      if (!overflowWarned) {
        DebugUIManager::GetInstance()->AddLog("LineRenderer: line buffer overflow, lines dropped", DebugUIManager::LogType::Error);
        overflowWarned = true;
      }
#endif
      return;
    }

    // 頂点データの設定
    lineData_->vertexData[lineIndex_].position = start;
    lineData_->vertexData[lineIndex_ + 1].position = end;

    // カラーデータの設定
    lineData_->vertexData[lineIndex_].color = color;
    lineData_->vertexData[lineIndex_ + 1].color = color;

    lineIndex_ += kVertexCountLine;
  }

  void LineRenderer::DrawArrow(const Vector3& start, const Vector3& end, const Vector4& color, float headSize)
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

  void LineRenderer::DrawSphere(const Vector3& center, const float radius, const Vector4& color, uint32_t subdivision)
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

  void LineRenderer::DrawAABB(const AABB& aabb, const Vector4& color)
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

  void LineRenderer::DrawOBB(const OBB& obb, const Vector4& color)
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

  void LineRenderer::DrawGrid(const float size, const float cellSize, const Vector4& color, float height)
  {
    // 手編集された設定ファイル由来の 0・負値・NaN を弾く
    if (!(size > 0.0f) || !(cellSize > 0.0f)) {
      return;
    }

    // 分割数を偶数にして原点を通る線を残し、マス幅を保つため全長の側を丸める
    const int   halfCount = static_cast<int>(std::clamp(std::round(size * 0.5f / cellSize), 1.0f, kGridMaxCellCount * 0.5f));
    const float halfWidth = cellSize * halfCount;

    for (int i = -halfCount; i <= halfCount; ++i) {
      const float offset = cellSize * i;
      DrawLine(Vector3(offset, height, halfWidth), Vector3(offset, height, -halfWidth), color);
      DrawLine(Vector3(halfWidth, height, offset), Vector3(-halfWidth, height, offset), color);
    }

    // X 軸
    DrawLine(Vector3(-halfWidth, height, 0.0f), Vector3(halfWidth, height, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f));
    // Z 軸
    DrawLine(Vector3(0.0f, height, -halfWidth), Vector3(0.0f, height, halfWidth), Vector4(0.0f, 0.0f, 1.0f, 1.0f));
    // Y 軸
    DrawLine(Vector3(0.0f, -halfWidth, 0.0f), Vector3(0.0f, halfWidth, 0.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f));

  }

  void LineRenderer::Draw()
  {
    DrawForView(camera_->GetViewMatrix() * camera_->GetProjectionMatrix());
  }

  void LineRenderer::DrawForView(const Matrix4x4& viewProjection)
  {
    DrawRange(*lineData_, 0, lineIndex_, viewProjection, pipeline_);
  }

  void LineRenderer::DrawRange(const LineData& lineData, uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection, const GraphicsPipelineDesc& pipeline)
  {
    if (vertexCount == 0) return;

    CommandContext& context = dx12_->GetContext();
    context.SetGraphicsPipeline(pipeline);
    dx12_->GetCommandList()->IASetVertexBuffers(0, 1, &lineData.vertexBufferView);
    context.Graphics().BindConstants("gTransformationMatrix"_id, TransformationMatrix{ viewProjection });

    // 全線分を1インスタンスで一括描画
    context.DrawInstanced(vertexCount, 1, startVertex, 0);
  }

  void LineRenderer::Reset()
  {
    lineIndex_ = 0;
  }

#ifdef _DEBUG
  void LineRenderer::DrawPreviewLines(const Matrix4x4& viewProjection)
  {
    // 同一フレームの先行描画分はまだ GPU が読むので上書きせず、その後ろだけ描く
    DrawRange(*previewLineData_, previewDrawStart_, previewLineIndex_ - previewDrawStart_, viewProjection, previewPipeline_);
    previewDrawStart_ = previewLineIndex_;
  }
#endif // _DEBUG

  void LineRenderer::CreatePipelines()
  {
    ShaderLibrary& library = dx12_->GetShaderLibrary();
    pipeline_.vs = library.Get(L"LineRenderer.VS.hlsl", ShaderStage::Vertex);
    pipeline_.ps = library.Get(L"LineRenderer.PS.hlsl", ShaderStage::Pixel);
    pipeline_.vertexLayout = VertexLayout::Line;
    pipeline_.renderState = { .blend = BlendMode::Alpha, .cull = CullMode::None, .depthTest = false, .depthWrite = false };
    pipeline_.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

#ifdef _DEBUG
    // プレビュー用: グリッドや視錐台がモデルの手前に透けないよう深度テストだけ行う
    previewPipeline_ = pipeline_;
    previewPipeline_.renderState.depthTest = true;
#endif // _DEBUG
  }

  void LineRenderer::CreateLineVertexData(LineData* lineData, uint32_t lineCount)
  {
    UINT vertexBufferSize = sizeof(VertexData) * kVertexCountLine * lineCount;

    // 頂点リソースを生成
    lineData->vertexBuffer = dx12_->MakeBufferResource(vertexBufferSize);

    // 頂点バッファビューを作成する
    lineData->vertexBufferView.BufferLocation = lineData->vertexBuffer->GetGPUVirtualAddress();
    lineData->vertexBufferView.SizeInBytes = vertexBufferSize;
    lineData->vertexBufferView.StrideInBytes = sizeof(VertexData);

    // 頂点リソースをマップ
    lineData->vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lineData->vertexData));
  }

} // namespace Tako
