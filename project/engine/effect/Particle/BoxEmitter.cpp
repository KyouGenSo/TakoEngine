#include "BoxEmitter.h"
#include "GPUParticle.h"

namespace Tako {

  BoxEmitter::BoxEmitter(GPUParticle* particleSystem, const Vector3& position,
    const Vector3& size, const Vector3& rotation,
    uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID を設定
  {
    // パラメータの初期化
    SetPosition(position);
    SetSize(size);
    SetRotation(rotation);
    SetParticleCount(count);
    SetFrequency(frequency);

    // エミッターのタイプを設定
    data_.type = static_cast<uint32_t>(EmitterType::Box);
  }

  std::shared_ptr<GPUParticleEmitter> BoxEmitter::Clone() const
  {
    auto clone = std::make_shared<BoxEmitter>(
      particleSystem_, GetPosition(), GetSize(), GetRotation(), GetParticleCount(), GetFrequency());
    CopyCommonStateTo(*clone);
    return clone;
  }

  void BoxEmitter::SetSize(const Vector3& size)
  {
    data_.boxSize = size;
  }

  void BoxEmitter::SetRotation(const Vector3& rotation)
  {
    data_.boxRotation = rotation;
  }

} // namespace Tako