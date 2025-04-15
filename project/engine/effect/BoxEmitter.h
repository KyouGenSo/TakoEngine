#pragma once
#include "GPUParticleEmitter.h"

class BoxEmitter : public GPUParticleEmitter
{
public:
  BoxEmitter(GPUParticle* particleSystem,
             const Vector3& position,
             const Vector3& size,
             const Vector3& rotation,
             uint32_t count,
             float frequency);

  ~BoxEmitter() override = default;

  // クローンメソッド
  std::shared_ptr<GPUParticleEmitter> Clone() const override;

  // GPUデータの設定
  void SetupGPUData(EmitterGPUData& gpuData) const override;

  // 箱型固有の設定
  void SetSize(const Vector3& size);
  void SetRotation(const Vector3& rotation);
  const Vector3& GetSize() const { return data_.box.size; }
  const Vector3 & GetRotation() const { return data_.box.rotation; }

  // 型情報
  EmitterType GetType() const override { return EmitterType::Box; }
};