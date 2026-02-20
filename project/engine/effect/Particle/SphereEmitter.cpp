#include "SphereEmitter.h"
#include "GPUParticle.h"

namespace Tako {

SphereEmitter::SphereEmitter(GPUParticle* particleSystem, const Vector3& position,
  float radius, uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的な ID を設定
{
  // パラメータの初期化
  SetPosition(position);
  SetRadius(radius);
  SetParticleCount(count);
  SetFrequency(frequency);

  // エミッターのタイプを設定
  data_.type = EmitterType::Sphere;
}

std::shared_ptr<GPUParticleEmitter> SphereEmitter::Clone() const
{
  // 新しいインスタンスを作成し、現在のデータをコピー
  auto clone = std::make_shared<SphereEmitter>(
    particleSystem_, GetPosition(), GetRadius(), GetParticleCount(), GetFrequency());

  // 他のプロパティも転送
  clone->SetColors(GetStartColor(), GetEndColor());
  clone->SetVelRange(GetVelRangeX(), GetVelRangeY(), GetVelRangeZ());
  clone->SetLifeTimeRange(GetLifeTimeRange());
  clone->SetScaleRange(GetScaleRangeX(), GetScaleRangeY());
  clone->SetActive(IsActive());
  clone->SetNormalize(IsNormalize());
  clone->SetRandomRotateZ(IsRandomRotateZ());
  clone->SetFrequencyTime(GetFrequency());

  return clone;
}

void SphereEmitter::SetupGPUData(EmitterGPUData& gpuData) const
{
  // 基底クラスのデータを設定
  GPUParticleEmitter::SetupGPUData(gpuData);
  // 球体固有のデータを設定
  gpuData.radius = data_.sphere.radius;
  gpuData.type = static_cast<uint32_t>(data_.type);
}

void SphereEmitter::SetRadius(float radius)
{
  data_.sphere.radius = radius;
}

} // namespace Tako
