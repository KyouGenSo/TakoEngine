#include "TriangleEmitter.h"
#include "GPUParticle.h"

TriangleEmitter::TriangleEmitter(GPUParticle* particleSystem, const Vector3& position,
  const Vector3& v1, const Vector3& v2, const Vector3& v3,
  uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
{
  // パラメータの初期化
  SetPosition(position);
  SetVertices(v1, v2, v3);
  SetParticleCount(count);
  SetFrequency(frequency);

  // エミッターのタイプを設定
  data_.type = EmitterType::Triangle;

}

void TriangleEmitter::SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3)
{
  data_.triangle.v1 = v1;
  data_.triangle.v2 = v2;
  data_.triangle.v3 = v3;
}
