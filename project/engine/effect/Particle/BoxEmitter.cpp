#include "BoxEmitter.h"
#include "GPUParticle.h"
#include <json.hpp>

namespace Tako {

  BoxEmitter::BoxEmitter(GPUParticle* particleSystem, const Vector3& position,
    const Vector3& size, const Vector3& rotation,
    uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID。RegisterEmitter で正式割り当て
  {
    SetPosition(position);
    SetSize(size);
    SetRotation(rotation);
    SetParticleCount(count);
    SetFrequency(frequency);

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

  void BoxEmitter::SerializeTypeSpecific(nlohmann::json& json) const
  {
    json["boxSize"] = { GetSize().x, GetSize().y, GetSize().z };
    json["boxRotation"] = { GetRotation().x, GetRotation().y, GetRotation().z };
  }

  std::shared_ptr<GPUParticleEmitter> BoxEmitter::CreateFromJSON(GPUParticle* particleSystem, const nlohmann::json& json)
  {
    const Vector3 position = { json["position"][0], json["position"][1], json["position"][2] };
    const Vector3 size = { json["boxSize"][0], json["boxSize"][1], json["boxSize"][2] };
    const Vector3 rotation = { json["boxRotation"][0], json["boxRotation"][1], json["boxRotation"][2] };
    const uint32_t count = json["particleCount"];
    const float frequency = json["frequency"];
    return std::make_shared<BoxEmitter>(particleSystem, position, size, rotation, count, frequency);
  }

} // namespace Tako