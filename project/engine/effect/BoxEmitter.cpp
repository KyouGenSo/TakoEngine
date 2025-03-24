#include "BoxEmitter.h"
#include "GPUParticle.h"

BoxEmitter::BoxEmitter(GPUParticle* particleSystem, const Vector3& position,
  const Vector3& size, const Vector3& rotation,
  uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
  , size_(size)
  , rotation_(rotation)
{
  // パラメータの初期化
  position_ = position;
  particleCount_ = count;
  frequency_ = frequency;

  // エミッターIDを実際の値に更新
  if (particleSystem_) {
    // IDを取得し、メンバーを更新
    uint32_t newId = particleSystem_->CreateBoxEmitterInternal(position, size, rotation, count, frequency);
    emitterId_ = newId;
  }
}

void BoxEmitter::SetSize(const Vector3& size)
{
  size_ = size;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.type = EmitterType::Box;
    params.box.size = size_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void BoxEmitter::SetRotation(const Vector3& rotation)
{
  rotation_ = rotation;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.type = EmitterType::Box;
    params.box.rotation = rotation_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}
