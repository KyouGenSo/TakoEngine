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
    data_.type = static_cast<uint32_t>(EmitterType::Sphere);
  }

  std::shared_ptr<GPUParticleEmitter> SphereEmitter::Clone() const
  {
    auto clone = std::make_shared<SphereEmitter>(
      particleSystem_, GetPosition(), GetRadius(), GetParticleCount(), GetFrequency());
    CopyCommonStateTo(*clone);
    return clone;
  }

  void SphereEmitter::SetRadius(float radius)
  {
    data_.radius = radius;
  }

} // namespace Tako
