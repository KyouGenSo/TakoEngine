#pragma once
#include "GPUParticleEmitter.h"

class TriangleEmitter : public GPUParticleEmitter
{
public:
  TriangleEmitter(
    GPUParticle* particleSystem,
    const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count,
    float frequency);

  ~TriangleEmitter() override = default;

  // 三角形固有の設定
  void SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3);
  const Vector3& GetVertex1() const { return data_.triangle.v1; }
  const Vector3& GetVertex2() const { return data_.triangle.v2; }
  const Vector3& GetVertex3() const { return data_.triangle.v3; }

  // 型情報
  EmitterType GetType() const override { return EmitterType::Triangle; }
};
