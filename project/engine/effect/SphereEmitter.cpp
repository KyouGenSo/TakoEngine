#include "SphereEmitter.h"
#include "GPUParticle.h"

SphereEmitter::SphereEmitter(GPUParticle* particleSystem, const Vector3& position,
  float radius, uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
  , radius_(radius)
{
  // パラメータの初期化
  position_ = position;
  particleCount_ = count;
  frequency_ = frequency;

  // エミッターIDを実際の値に更新
  if (particleSystem_) {
    // IDを取得し、メンバーを更新
    uint32_t newId = particleSystem_->CreateSphereEmitterInternal(position, radius, count, frequency);
    emitterId_ = newId;
  }
}

void SphereEmitter::SetRadius(float radius)
{
  radius_ = radius;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.type = EmitterType::Sphere;
    params.sphere.radius = radius_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}
