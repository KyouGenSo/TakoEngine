#include "TriangleEmitter.h"
#include "GPUParticle.h"

TriangleEmitter::TriangleEmitter(GPUParticle* particleSystem, const Vector3& position,
  const Vector3& v1, const Vector3& v2, const Vector3& v3,
  uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
  , vertex1_(v1)
  , vertex2_(v2)
  , vertex3_(v3)
{
  // パラメータの初期化
  position_ = position;
  particleCount_ = count;
  frequency_ = frequency;

  // エミッターIDを実際の値に更新
  if (particleSystem_) {
    // IDを取得し、メンバーを更新
    uint32_t newId = particleSystem_->CreateTriangleEmitterInternal(position, v1, v2, v3, count, frequency);
    emitterId_ = newId;
  }
}

void TriangleEmitter::SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3)
{
  vertex1_ = v1;
  vertex2_ = v2;
  vertex3_ = v3;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.type = EmitterType::Triangle;
    params.triangle.v1 = vertex1_;
    params.triangle.v2 = vertex2_;
    params.triangle.v3 = vertex3_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}
