#include "MeshEmitter.h"
#include "GPUParticle.h"
#include "Mesh.h"
#include "Model.h"
#include "Object3d.h"
#include "SrvManager.h"
#include "Mat4x4Func.h"
#include "DX12Basic.h"
#include "ModelStruct.h"
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>
#include <json.hpp>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  MeshEmitter::SharedBufferSrv::~SharedBufferSrv()
  {
    SrvManager::GetInstance()->Free(srvIndex);
  }

  MeshEmitter::MeshEmitter(GPUParticle* particleSystem, Model* model, uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID (RegisterEmitter で正式割り当て)
  {
    SetParticleCount(count);
    SetFrequency(frequency);
    data_.type = static_cast<uint32_t>(EmitterType::Mesh);
    data_.meshWorld = Mat4x4::MakeIdentity();
    BuildFromModel(model);
  }

  void MeshEmitter::BuildFromModel(Model* model)
  {
    // model = nullptr は Clone 用の空殻生成 (状態は呼び出し側がコピーする)
    if (model == nullptr || model->GetMeshCount() == 0) return;

    SrvManager* srvManager = particleSystem_->GetSrvManager();
    DX12Basic* dx12 = particleSystem_->GetDx12();
    if (srvManager == nullptr || dx12 == nullptr) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "MeshEmitter: SrvManager/DX12Basic unavailable. Spawn shape not built.",
        DebugUIManager::LogType::Error);
#endif
      return;
    }

    const size_t meshCount = model->GetMeshCount();

    // Mesh 1 個ならスキニング対応のため集約せず Mesh の SRV を共有
    if (meshCount == 1) {
      Mesh* mesh = model->GetMesh(0);
      if (mesh == nullptr) return;
      mesh_ = mesh;

      data_.meshVertexSrvIndex = mesh->GetVertexSrvIndex();
      data_.meshIndexSrvIndex = mesh->GetIndexSrvIndex(); // Mesh の遅延生成 getter から共有取得
      data_.meshTriangleCount = mesh->GetIndexCount() / 3u;
      data_.meshAabbMin = mesh->GetAABBLocalMin();
      data_.meshAabbMax = mesh->GetAABBLocalMax();
      if (mesh->HasSkinning()) {
        data_.meshSkinnedVertexSrvIndex = mesh->GetSkinnedVertexSrvIndex();
      }

      const std::vector<float> prefixSum = ComputeTriangleAreaPrefixSum(mesh->GetVertices(), mesh->GetIndices());
      if (prefixSum.size() > 1u) {
        data_.meshTotalArea = prefixSum.back();
        areaPrefixSum_ = CreateStructuredBufferSrv(
          prefixSum.data(), sizeof(float), static_cast<uint32_t>(prefixSum.size()));
        data_.meshAreaPrefixSumSrvIndex = areaPrefixSum_->srvIndex;
      }
      return;
    }

    // 複数 Mesh: indices に vertex base offset を加算しながら 1 本に連結
    std::vector<VertexData> aggregatedVertices;
    std::vector<uint32_t> aggregatedIndices;

    Vector3 aggAabbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    Vector3 aggAabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool hasSkinnedMesh = false;

    for (size_t m = 0; m < meshCount; ++m) {
      Mesh* mesh = model->GetMesh(m);
      if (mesh == nullptr) continue;
      const auto& vertices = mesh->GetVertices();
      const auto& indices = mesh->GetIndices();
      if (vertices.empty() || indices.empty()) continue;

      hasSkinnedMesh |= mesh->HasSkinning();

      const uint32_t vertexBaseOffset = static_cast<uint32_t>(aggregatedVertices.size());

      aggregatedVertices.insert(aggregatedVertices.end(), vertices.begin(), vertices.end());

      const size_t prevIndexSize = aggregatedIndices.size();
      aggregatedIndices.resize(prevIndexSize + indices.size());
      for (size_t k = 0; k < indices.size(); ++k) {
        aggregatedIndices[prevIndexSize + k] = indices[k] + vertexBaseOffset;
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

    if (aggregatedVertices.empty() || aggregatedIndices.empty()) {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "MeshEmitter: model has no valid mesh data. Spawn shape not built.",
        DebugUIManager::LogType::Warning);
#endif
      return;
    }

#ifdef _DEBUG
    if (hasSkinnedMesh) {
      DebugUIManager::GetInstance()->AddLog(
        "MeshEmitter: aggregated mode does not support skinning. Spawning from bind pose.",
        DebugUIManager::LogType::Warning);
    }
#endif

    aggregatedVertex_ = CreateStructuredBufferSrv(
      aggregatedVertices.data(), sizeof(VertexData), static_cast<uint32_t>(aggregatedVertices.size()));
    data_.meshVertexSrvIndex = aggregatedVertex_->srvIndex;
    aggregatedIndex_ = CreateStructuredBufferSrv(
      aggregatedIndices.data(), sizeof(uint32_t), static_cast<uint32_t>(aggregatedIndices.size()));
    data_.meshIndexSrvIndex = aggregatedIndex_->srvIndex;

    const std::vector<float> prefixSum = ComputeTriangleAreaPrefixSum(aggregatedVertices, aggregatedIndices);
    if (prefixSum.size() > 1u) {
      data_.meshTotalArea = prefixSum.back();
      areaPrefixSum_ = CreateStructuredBufferSrv(
        prefixSum.data(), sizeof(float), static_cast<uint32_t>(prefixSum.size()));
      data_.meshAreaPrefixSumSrvIndex = areaPrefixSum_->srvIndex;
    }

    data_.meshTriangleCount = static_cast<uint32_t>(aggregatedIndices.size() / 3u);
    data_.meshAabbMin = aggAabbMin;
    data_.meshAabbMax = aggAabbMax;
    // 集約モードではスキニング動的同期は未対応 (バインドポーズで固定)
    data_.meshSkinnedVertexSrvIndex = 0;
  }

  std::vector<float> MeshEmitter::ComputeTriangleAreaPrefixSum(
    const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices)
  {
    const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3u);
    std::vector<float> prefixSum(triCount + 1u, 0.0f);
    for (uint32_t t = 0; t < triCount; ++t) {
      const auto& p0 = vertices[indices[t * 3u + 0u]].position;
      const auto& p1 = vertices[indices[t * 3u + 1u]].position;
      const auto& p2 = vertices[indices[t * 3u + 2u]].position;
      // 三角形面積 = 0.5 * |cross(p1-p0, p2-p0)|
      const float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
      const float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;
      const float cx = e1y * e2z - e1z * e2y;
      const float cy = e1z * e2x - e1x * e2z;
      const float cz = e1x * e2y - e1y * e2x;
      const float area = 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
      prefixSum[t + 1u] = prefixSum[t] + area;
    }
    return prefixSum;
  }

  std::shared_ptr<MeshEmitter::SharedBufferSrv> MeshEmitter::CreateStructuredBufferSrv(
    const void* srcData, size_t elementSize, uint32_t elementCount)
  {
    DX12Basic* dx12 = particleSystem_->GetDx12();
    SrvManager* srvManager = particleSystem_->GetSrvManager();

    auto buffer = std::make_shared<SharedBufferSrv>();

    const size_t bufferSize = elementSize * elementCount;
    dx12->CreateBufferResource(buffer->resource, bufferSize);
    void* mapped = nullptr;
    buffer->resource->Map(0, nullptr, &mapped);
    std::memcpy(mapped, srcData, bufferSize);
    buffer->resource->Unmap(0, nullptr);

    buffer->srvIndex = srvManager->Allocate();
    srvManager->CreateSRVForStructuredBuffer(
      buffer->srvIndex, buffer->resource.Get(), elementCount, static_cast<UINT>(elementSize));
    return buffer;
  }

  std::shared_ptr<GPUParticleEmitter> MeshEmitter::Clone() const
  {
    // 空殻 (model = nullptr) を生成して全状態をコピーする。スポーン形状は再構築しない。
    auto clone = std::make_shared<MeshEmitter>(particleSystem_, nullptr, data_.count, data_.frequency);
    CopyCommonStateTo(*clone); // data_ 全体 (mesh SRV index 含む) + renderModelPath_ を転送
    clone->mesh_ = mesh_;
    clone->boundObject3d_ = boundObject3d_;
    clone->spawnModelPath_ = spawnModelPath_;
    clone->offsetRotation_ = offsetRotation_;
    clone->offsetScale_ = offsetScale_;
    // GPU リソースは構築後 immutable なので shared_ptr 共有。最終所有者の破棄時に 1 回だけ SRV が返却される
    clone->areaPrefixSum_ = areaPrefixSum_;
    clone->aggregatedVertex_ = aggregatedVertex_;
    clone->aggregatedIndex_ = aggregatedIndex_;
    return clone;
  }

  void MeshEmitter::SerializeTypeSpecific(nlohmann::json& json) const
  {
    // スポーン形状はモデルパスで自己完結保存する。Object3d バインドは実行時情報のため
    // 永続化せず、復元時に呼び出し側が LoadPreset(presetName, newName, obj3d) で再バインドする。
    if (!spawnModelPath_.empty()) {
      json["meshModelPath"] = spawnModelPath_;
    }
#ifdef _DEBUG
    else {
      DebugUIManager::GetInstance()->AddLog(
        "Serialize: MeshEmitter has no meshModelPath; restorable only via LoadPreset(Object3d*).",
        DebugUIManager::LogType::Warning);
    }
#endif
    json["meshOffsetRotation"] = { offsetRotation_.x, offsetRotation_.y, offsetRotation_.z };
    json["meshOffsetScale"] = { offsetScale_.x, offsetScale_.y, offsetScale_.z };
  }

  void MeshEmitter::DeserializeTypeSpecific(const nlohmann::json& json)
  {
    if (json.contains("meshOffsetRotation")) {
      const Vector3 offsetRotation = {
        json["meshOffsetRotation"][0], json["meshOffsetRotation"][1], json["meshOffsetRotation"][2] };
      SetOffsetRotation(offsetRotation);
    }
    if (json.contains("meshOffsetScale")) {
      const Vector3 offsetScale = {
        json["meshOffsetScale"][0], json["meshOffsetScale"][1], json["meshOffsetScale"][2] };
      SetOffsetScale(offsetScale);
    }
  }

  std::shared_ptr<GPUParticleEmitter> MeshEmitter::CreateFromJSON(
    GPUParticle* particleSystem, const nlohmann::json& json, Object3d* bindTarget)
  {
    const uint32_t count = json["particleCount"];
    const float frequency = json["frequency"];

    std::shared_ptr<MeshEmitter> emitter;
    if (bindTarget != nullptr) {
      // バインド先のモデルをスポーン形状に使い、毎フレーム world 行列に追従させる
      emitter = std::make_shared<MeshEmitter>(particleSystem, bindTarget->GetModel(), count, frequency);
      emitter->BindObject3d(bindTarget);
    }
    else if (json.contains("meshModelPath") && !json["meshModelPath"].get<std::string>().empty()) {
      // モデルパスから自己完結で復元 (エディタ作成の Mesh エミッター用)
      const std::string meshModelPath = json["meshModelPath"].get<std::string>();
      Model* model = particleSystem->AcquireModel(meshModelPath);
      if (model == nullptr) {
#ifdef _DEBUG
        DebugUIManager::GetInstance()->AddLog(
          "Deserialize: failed to load meshModelPath '" + meshModelPath + "'.",
          DebugUIManager::LogType::Error);
#endif
        return nullptr;
      }
      emitter = std::make_shared<MeshEmitter>(particleSystem, model, count, frequency);
      emitter->SetSpawnModelPath(meshModelPath);
    }
    else {
#ifdef _DEBUG
      DebugUIManager::GetInstance()->AddLog(
        "Deserialize: Mesh emitter has no 'meshModelPath' and no bind target. Skipping.",
        DebugUIManager::LogType::Warning);
#endif
      return nullptr;
    }

    // position はローカルオフセット平行移動
    const Vector3 position = { json["position"][0], json["position"][1], json["position"][2] };
    emitter->SetPosition(position);
    emitter->DeserializeTypeSpecific(json);
    return emitter;
  }

  void MeshEmitter::SyncMeshWorld()
  {
    // ローカルオフセットをバインド先のローカル空間で適用。
    const Matrix4x4 offset = Mat4x4::MakeAffine(offsetScale_, offsetRotation_, data_.position);
    data_.meshWorld = (boundObject3d_ != nullptr)
      ? Mat4x4::Multiply(offset, boundObject3d_->GetWorldMatrix())
      : offset;
  }

  void MeshEmitter::UpdateEmission(float deltaTime)
  {
    // 基底クラスで active/emitting タイマー + targetPosition 動的同期を処理
    GPUParticleEmitter::UpdateEmission(deltaTime);
    // meshWorld をオフセット + バインド先 world から再合成
    SyncMeshWorld();
  }

} // namespace Tako
