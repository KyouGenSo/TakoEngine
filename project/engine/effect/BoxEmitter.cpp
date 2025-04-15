#include "BoxEmitter.h"
#include "GPUParticle.h"

BoxEmitter::BoxEmitter(GPUParticle* particleSystem, const Vector3& position,
                       const Vector3& size, const Vector3& rotation,
                       uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
{
  // パラメータの初期化
  SetPosition(position);
  SetSize(size);
  SetRotation(rotation);
  SetParticleCount(count);
  SetFrequency(frequency);

  // エミッターのタイプを設定
  data_.type = EmitterType::Box;
}

void BoxEmitter::SetSize(const Vector3& size)
{
  data_.box.size = size;
}

void BoxEmitter::SetRotation(const Vector3& rotation)
{
  data_.box.rotation = rotation;
}