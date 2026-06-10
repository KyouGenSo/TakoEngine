#include "SphereEmitter.h"
#include "GPUParticle.h"
#include <json.hpp>

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

  void SphereEmitter::SerializeTypeSpecific(nlohmann::json& json) const
  {
    json["radius"] = GetRadius();
  }

  std::shared_ptr<GPUParticleEmitter> SphereEmitter::CreateFromJSON(GPUParticle* particleSystem, const nlohmann::json& json)
  {
    const Vector3 position = { json["position"][0], json["position"][1], json["position"][2] };
    const float radius = json["radius"];
    const uint32_t count = json["particleCount"];
    const float frequency = json["frequency"];
    return std::make_shared<SphereEmitter>(particleSystem, position, radius, count, frequency);
  }

} // namespace Tako
