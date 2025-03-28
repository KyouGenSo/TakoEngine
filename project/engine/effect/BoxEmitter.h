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

  // 箱型固有の設定
  void SetSize(const Vector3& size);
  void SetRotation(const Vector3& rotation);
  const Vector3& GetSize() const { return size_; }
  const Vector3& GetRotation() const { return rotation_; }

  // 型情報
  EmitterType GetType() const override { return EmitterType::Box; }

private:
  Vector3 size_;
  Vector3 rotation_;
};