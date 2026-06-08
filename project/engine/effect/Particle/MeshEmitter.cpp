#include "MeshEmitter.h"
#include "GPUParticle.h"
#include "Mesh.h"
#include "Model.h"
#include "Object3d.h"
#include "SrvManager.h"
#include "Matrix4x4.h"
#include "Mat4x4Func.h"
#include "DX12Basic.h"
#include "ModelStruct.h"
#include <cstring>
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

namespace Tako {

  MeshEmitter::MeshEmitter(GPUParticle* particleSystem, Mesh* mesh, uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID (RegisterEmitter で正式割り当て)
    , mesh_(mesh)
  {
    SetParticleCount(count);
    SetFrequency(frequency);
    data_.type = static_cast<uint32_t>(EmitterType::Mesh);

    if (mesh_ != nullptr) {
      SrvManager* srvManager = particleSystem_->GetSrvManager();
      meshIndexSrvIndex_ = mesh_->GetIndexSrvIndex();

      // EmitterData にメッシュ情報を反映
      data_.meshVertexSrvIndex = mesh_->GetVertexSrvIndex();
      data_.meshIndexSrvIndex = meshIndexSrvIndex_;
      data_.meshTriangleCount = mesh_->GetIndexCount() / 3u;
      data_.meshAabbMin = mesh_->GetAABBLocalMin();
      data_.meshAabbMax = mesh_->GetAABBLocalMax();

      if (mesh_->HasSkinning()) {
        data_.meshSkinnedVertexSrvIndex = mesh_->GetSkinnedVertexSrvIndex();
      }

      // 三角形面積 Prefix Sum (Inversion Sampling)
      const auto& vertices = mesh_->GetVertices();
      const auto& indices = mesh_->GetIndices();
      const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3u);
      DX12Basic* dx12 = particleSystem_->GetDx12();
      if (triCount > 0 && dx12 != nullptr && srvManager != nullptr) {
        std::vector<float> prefixSum(triCount + 1u);
        prefixSum[0] = 0.0f;
        for (uint32_t t = 0; t < triCount; ++t) {
          const uint32_t i0 = indices[t * 3u + 0u];
          const uint32_t i1 = indices[t * 3u + 1u];
          const uint32_t i2 = indices[t * 3u + 2u];
          const auto& p0 = vertices[i0].position;
          const auto& p1 = vertices[i1].position;
          const auto& p2 = vertices[i2].position;
          // 三角形面積 = 0.5 * |cross(p1-p0, p2-p0)|
          const float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
          const float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;
          const float cx = e1y * e2z - e1z * e2y;
          const float cy = e1z * e2x - e1x * e2z;
          const float cz = e1x * e2y - e1y * e2x;
          const float area = 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
          prefixSum[t + 1u] = prefixSum[t] + area;
        }
        data_.meshTotalArea = prefixSum[triCount];

        // UPLOAD バッファに prefix sum を書き込み
        const size_t bufferSize = sizeof(float) * (triCount + 1u);
        dx12->CreateBufferResource(areaPrefixSumResource_, bufferSize);
        void* mapped = nullptr;
        areaPrefixSumResource_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, prefixSum.data(), bufferSize);
        areaPrefixSumResource_->Unmap(0, nullptr);

        // SRV 登録
        meshAreaPrefixSumSrvIndex_ = srvManager->Allocate();
        srvManager->CreateSRVForStructuredBuffer(
          meshAreaPrefixSumSrvIndex_,
          areaPrefixSumResource_.Get(),
          triCount + 1u,
          static_cast<UINT>(sizeof(float)));
        data_.meshAreaPrefixSumSrvIndex = meshAreaPrefixSumSrvIndex_;
      }
    }

    // 初期 world は単位行列 (BindMeshWorld / SetMeshWorld で更新)
    data_.meshWorld = Mat4x4::MakeIdentity();
  }

  MeshEmitter::MeshEmitter(GPUParticle* particleSystem, Model* model, uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0)
    , mesh_(nullptr)
  {
    SetParticleCount(count);
    SetFrequency(frequency);
    data_.type = static_cast<uint32_t>(EmitterType::Mesh);
    data_.meshWorld = Mat4x4::MakeIdentity();

    if (model == nullptr || model->GetMeshCount() == 0) return;

    SrvManager* srvManager = particleSystem_->GetSrvManager();
    DX12Basic* dx12 = particleSystem_->GetDx12();
    if (srvManager == nullptr || dx12 == nullptr) return;

    const size_t meshCount = model->GetMeshCount();

    // Mesh 1 個ならスキニング対応のため集約せず単一 SRV を流用
    if (meshCount == 1) {
      Mesh* singleMesh = model->GetMesh(0);
      if (singleMesh == nullptr) return;
      mesh_ = singleMesh;

      // index SRV は Mesh の遅延生成 getter から共有取得
      meshIndexSrvIndex_ = singleMesh->GetIndexSrvIndex();

      data_.meshVertexSrvIndex = singleMesh->GetVertexSrvIndex();
      data_.meshIndexSrvIndex = meshIndexSrvIndex_;
      data_.meshTriangleCount = singleMesh->GetIndexCount() / 3u;
      data_.meshAabbMin = singleMesh->GetAABBLocalMin();
      data_.meshAabbMax = singleMesh->GetAABBLocalMax();
      if (singleMesh->HasSkinning()) {
        data_.meshSkinnedVertexSrvIndex = singleMesh->GetSkinnedVertexSrvIndex();
      }

      // Prefix Sum
      const auto& vertices = singleMesh->GetVertices();
      const auto& indices = singleMesh->GetIndices();
      const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3u);
      if (triCount > 0) {
        std::vector<float> prefixSum(triCount + 1u, 0.0f);
        for (uint32_t t = 0; t < triCount; ++t) {
          const auto& p0 = vertices[indices[t * 3u + 0u]].position;
          const auto& p1 = vertices[indices[t * 3u + 1u]].position;
          const auto& p2 = vertices[indices[t * 3u + 2u]].position;
          const float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
          const float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;
          const float cx = e1y * e2z - e1z * e2y;
          const float cy = e1z * e2x - e1x * e2z;
          const float cz = e1x * e2y - e1y * e2x;
          prefixSum[t + 1u] = prefixSum[t] + 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
        }
        data_.meshTotalArea = prefixSum[triCount];
        const size_t bufferSize = sizeof(float) * (triCount + 1u);
        dx12->CreateBufferResource(areaPrefixSumResource_, bufferSize);
        void* mapped = nullptr;
        areaPrefixSumResource_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, prefixSum.data(), bufferSize);
        areaPrefixSumResource_->Unmap(0, nullptr);
        meshAreaPrefixSumSrvIndex_ = srvManager->Allocate();
        srvManager->CreateSRVForStructuredBuffer(
          meshAreaPrefixSumSrvIndex_,
          areaPrefixSumResource_.Get(),
          triCount + 1u,
          static_cast<UINT>(sizeof(float)));
        data_.meshAreaPrefixSumSrvIndex = meshAreaPrefixSumSrvIndex_;
      }
      return;
    }

    // 複数 Mesh: indices に vertex base offset を加算しながら 1 本に連結
    std::vector<VertexData> aggregatedVertices;
    std::vector<uint32_t> aggregatedIndices;
    std::vector<float> prefixSum;
    prefixSum.push_back(0.0f);

    Vector3 aggAabbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    Vector3 aggAabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (size_t m = 0; m < meshCount; ++m) {
      Mesh* mesh = model->GetMesh(m);
      if (mesh == nullptr) continue;
      const auto& vertices = mesh->GetVertices();
      const auto& indices = mesh->GetIndices();
      if (vertices.empty() || indices.empty()) continue;

      const uint32_t vertexBaseOffset = static_cast<uint32_t>(aggregatedVertices.size());

      aggregatedVertices.insert(aggregatedVertices.end(), vertices.begin(), vertices.end());

      const size_t prevIndexSize = aggregatedIndices.size();
      aggregatedIndices.resize(prevIndexSize + indices.size());
      for (size_t k = 0; k < indices.size(); ++k) {
        aggregatedIndices[prevIndexSize + k] = indices[k] + vertexBaseOffset;
      }

      const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3u);
      for (uint32_t t = 0; t < triCount; ++t) {
        const auto& p0 = vertices[indices[t * 3u + 0u]].position;
        const auto& p1 = vertices[indices[t * 3u + 1u]].position;
        const auto& p2 = vertices[indices[t * 3u + 2u]].position;
        const float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
        const float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;
        const float cx = e1y * e2z - e1z * e2y;
        const float cy = e1z * e2x - e1x * e2z;
        const float cz = e1x * e2y - e1y * e2x;
        const float area = 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
        prefixSum.push_back(prefixSum.back() + area);
      }

      // Windows.h の min/max マクロが std::min/std::max と衝突するため括弧で展開抑止
      const auto& meshMin = mesh->GetAABBLocalMin();
      const auto& meshMax = mesh->GetAABBLocalMax();
      aggAabbMin.x = (std::min)(aggAabbMin.x, meshMin.x);
      aggAabbMin.y = (std::min)(aggAabbMin.y, meshMin.y);
      aggAabbMin.z = (std::min)(aggAabbMin.z, meshMin.z);
      aggAabbMax.x = (std::max)(aggAabbMax.x, meshMax.x);
      aggAabbMax.y = (std::max)(aggAabbMax.y, meshMax.y);
      aggAabbMax.z = (std::max)(aggAabbMax.z, meshMax.z);
    }

    if (aggregatedVertices.empty() || aggregatedIndices.empty()) return;

    {
      const size_t bufferSize = sizeof(VertexData) * aggregatedVertices.size();
      dx12->CreateBufferResource(aggregatedVertexResource_, bufferSize);
      void* mapped = nullptr;
      aggregatedVertexResource_->Map(0, nullptr, &mapped);
      std::memcpy(mapped, aggregatedVertices.data(), bufferSize);
      aggregatedVertexResource_->Unmap(0, nullptr);

      aggregatedVertexSrvIndex_ = srvManager->Allocate();
      srvManager->CreateSRVForStructuredBuffer(
        aggregatedVertexSrvIndex_,
        aggregatedVertexResource_.Get(),
        static_cast<UINT>(aggregatedVertices.size()),
        static_cast<UINT>(sizeof(VertexData)));
    }

    {
      const size_t bufferSize = sizeof(uint32_t) * aggregatedIndices.size();
      dx12->CreateBufferResource(aggregatedIndexResource_, bufferSize);
      void* mapped = nullptr;
      aggregatedIndexResource_->Map(0, nullptr, &mapped);
      std::memcpy(mapped, aggregatedIndices.data(), bufferSize);
      aggregatedIndexResource_->Unmap(0, nullptr);

      aggregatedIndexSrvIndex_ = srvManager->Allocate();
      srvManager->CreateSRVForStructuredBuffer(
        aggregatedIndexSrvIndex_,
        aggregatedIndexResource_.Get(),
        static_cast<UINT>(aggregatedIndices.size()),
        static_cast<UINT>(sizeof(uint32_t)));
    }

    const uint32_t aggTriCount = static_cast<uint32_t>(aggregatedIndices.size() / 3u);
    if (aggTriCount > 0) {
      const size_t bufferSize = sizeof(float) * (aggTriCount + 1u);
      dx12->CreateBufferResource(areaPrefixSumResource_, bufferSize);
      void* mapped = nullptr;
      areaPrefixSumResource_->Map(0, nullptr, &mapped);
      std::memcpy(mapped, prefixSum.data(), bufferSize);
      areaPrefixSumResource_->Unmap(0, nullptr);

      meshAreaPrefixSumSrvIndex_ = srvManager->Allocate();
      srvManager->CreateSRVForStructuredBuffer(
        meshAreaPrefixSumSrvIndex_,
        areaPrefixSumResource_.Get(),
        aggTriCount + 1u,
        static_cast<UINT>(sizeof(float)));
    }

    data_.meshVertexSrvIndex = aggregatedVertexSrvIndex_;
    data_.meshIndexSrvIndex = aggregatedIndexSrvIndex_;
    data_.meshTriangleCount = aggTriCount;
    data_.meshAabbMin = aggAabbMin;
    data_.meshAabbMax = aggAabbMax;
    data_.meshAreaPrefixSumSrvIndex = meshAreaPrefixSumSrvIndex_;
    data_.meshTotalArea = prefixSum.back();
    // 集約モードではスキニング動的同期は未対応 (バインドポーズで固定)
    data_.meshSkinnedVertexSrvIndex = 0;
  }

  MeshEmitter::MeshEmitter(GPUParticle* particleSystem, Object3d* obj3d,
                           uint32_t count, float frequency, std::string object3dKey)
    : MeshEmitter(particleSystem, obj3d ? obj3d->GetModel() : nullptr, count, frequency)
  {
    boundObject3d_ = obj3d;
    object3dKey_ = std::move(object3dKey);
  }

  std::shared_ptr<GPUParticleEmitter> MeshEmitter::Clone() const
  {
    auto clone = std::make_shared<MeshEmitter>(particleSystem_, mesh_, data_.count, data_.frequency);
    // 全パラメータをコピー (ただし SRV インデックスはクローン側の新規 SRV を保持)
    uint32_t cloneSrvIndex = clone->data_.meshIndexSrvIndex;
    clone->data_ = data_;
    clone->data_.meshIndexSrvIndex = cloneSrvIndex;
    CopyDrawStateTo(*clone);                    // renderModelPath_ 等(data_ コピーで漏れる文字列メンバ)を転送
    clone->SetSpawnModelPath(spawnModelPath_);  // スポーン形状モデルパスを転送
    return clone;
  }

  void MeshEmitter::SetMeshWorld(const Matrix4x4& world)
  {
    data_.meshWorld = world;
    // 静的指定なので動的バインドは解除
    boundMeshWorld_ = nullptr;
  }

  void MeshEmitter::SyncMeshWorld()
  {
    // Object3d バインドが優先 (動的世界行列を毎回再計算で取得)
    if (boundObject3d_ != nullptr) {
      data_.meshWorld = boundObject3d_->GetWorldMatrix();
      return;
    }
    if (boundMeshWorld_ != nullptr) {
      data_.meshWorld = *boundMeshWorld_;
    }
  }

  void MeshEmitter::UpdateEmission(float deltaTime)
  {
    // 基底クラスで active/emitting タイマー + targetPosition 動的同期を処理
    GPUParticleEmitter::UpdateEmission(deltaTime);
    // メッシュの world 行列を動的同期 (BindMeshWorld されている場合のみ)
    SyncMeshWorld();
  }

} // namespace Tako
