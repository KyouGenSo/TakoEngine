#include "MeshEmitter.h"
#include "GPUParticle.h"
#include "Mesh.h"
#include "SrvManager.h"
#include "Matrix4x4.h"
#include "Mat4x4Func.h"

namespace Tako {

  MeshEmitter::MeshEmitter(GPUParticle* particleSystem, Mesh* mesh, uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID (RegisterEmitter で正式割り当て)
    , mesh_(mesh)
  {
    SetParticleCount(count);
    SetFrequency(frequency);
    data_.type = static_cast<uint32_t>(EmitterType::Mesh);

    if (mesh_ != nullptr) {
      // Stage D-1: メッシュのインデックスバッファを StructuredBuffer<uint> として SRV 化
      // 頂点バッファはすでに Mesh 内部で SRV 化されている (vertexSrvIndex_)
      SrvManager* srvManager = particleSystem_->GetSrvManager();
      if (srvManager != nullptr) {
        meshIndexSrvIndex_ = srvManager->Allocate();
        srvManager->CreateSRVForStructuredBuffer(
          meshIndexSrvIndex_,
          mesh_->GetIndexResource(),
          mesh_->GetIndexCount(),
          sizeof(uint32_t));
      }

      // EmitterData にメッシュ情報を反映
      data_.meshVertexSrvIndex = mesh_->GetVertexSrvIndex();
      data_.meshIndexSrvIndex = meshIndexSrvIndex_;
      data_.meshTriangleCount = mesh_->GetIndexCount() / 3u;
      data_.meshAabbMin = mesh_->GetAABBLocalMin();
      data_.meshAabbMax = mesh_->GetAABBLocalMax();
    }

    // 初期 world は単位行列 (BindMeshWorld / SetMeshWorld で更新)
    data_.meshWorld = Mat4x4::MakeIdentity();
  }

  std::shared_ptr<GPUParticleEmitter> MeshEmitter::Clone() const
  {
    auto clone = std::make_shared<MeshEmitter>(particleSystem_, mesh_, data_.count, data_.frequency);
    // 全パラメータをコピー (ただし SRV インデックスはクローン側の新規 SRV を保持)
    uint32_t cloneSrvIndex = clone->data_.meshIndexSrvIndex;
    clone->data_ = data_;
    clone->data_.meshIndexSrvIndex = cloneSrvIndex;
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
    if (boundMeshWorld_ != nullptr) {
      data_.meshWorld = *boundMeshWorld_;
    }
  }

  void MeshEmitter::UpdateEmission(float deltaTime)
  {
    // 基底クラスで active/emitting タイマー + Stage C の targetPosition 動的同期を処理
    GPUParticleEmitter::UpdateEmission(deltaTime);
    // Stage D-1: メッシュの world 行列を動的同期 (BindMeshWorld されている場合のみ)
    SyncMeshWorld();
  }

} // namespace Tako
