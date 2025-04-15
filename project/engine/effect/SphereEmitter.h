#pragma once
#include "GPUParticleEmitter.h"

class SphereEmitter : public GPUParticleEmitter
{
public:
  SphereEmitter(
    GPUParticle* particleSystem,
    const Vector3& position,
    float radius,
    uint32_t count,
    float frequency);

  ~SphereEmitter() override = default;

  // クローンメソッド
  std::shared_ptr<GPUParticleEmitter> Clone() const override;

  // GPUデータの設定
  void SetupGPUData(EmitterGPUData& gpuData) const override;

  // 球体固有の設定
  void SetRadius(float radius);
  float GetRadius() const { return data_.sphere.radius; }

  // 型情報
  EmitterType GetType() const override { return EmitterType::Sphere; }
};

