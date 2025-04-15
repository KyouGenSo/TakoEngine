#include "SphereEmitter.h"
#include "GPUParticle.h"

SphereEmitter::SphereEmitter(GPUParticle* particleSystem, const Vector3& position,
  float radius, uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
{
  // パラメータの初期化
  SetPosition(position);
  SetRadius(radius);
  SetParticleCount(count);
  SetFrequency(frequency);

  // エミッターのタイプを設定
  data_.type = EmitterType::Sphere;
}

void SphereEmitter::SetRadius(float radius)
{
  data_.sphere.radius = radius;
}
