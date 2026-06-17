#include "TriangleEmitter.h"
#include "GPUParticle.h"
#include <json.hpp>

namespace Tako {

  TriangleEmitter::TriangleEmitter(GPUParticle* particleSystem, const Vector3& position,
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    uint32_t count, float frequency)
    : GPUParticleEmitter(particleSystem, 0) // 一時的な ID。RegisterEmitter で正式割り当て
  {
    SetPosition(position);
    SetVertices(v1, v2, v3);
    SetParticleCount(count);
    SetFrequency(frequency);

    data_.type = static_cast<uint32_t>(EmitterType::Triangle);

  }

  std::shared_ptr<GPUParticleEmitter> TriangleEmitter::Clone() const
  {
    auto clone = std::make_shared<TriangleEmitter>(
      particleSystem_, GetPosition(), GetVertex1(), GetVertex2(), GetVertex3(), GetParticleCount(), GetFrequency());
    CopyCommonStateTo(*clone);
    return clone;
  }

  void TriangleEmitter::SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3)
  {
    data_.triangleV1 = v1;
    data_.triangleV2 = v2;
    data_.triangleV3 = v3;
  }

  void TriangleEmitter::SerializeTypeSpecific(nlohmann::json& json) const
  {
    json["triangleV1"] = { GetVertex1().x, GetVertex1().y, GetVertex1().z };
    json["triangleV2"] = { GetVertex2().x, GetVertex2().y, GetVertex2().z };
    json["triangleV3"] = { GetVertex3().x, GetVertex3().y, GetVertex3().z };
  }

  std::shared_ptr<GPUParticleEmitter> TriangleEmitter::CreateFromJSON(GPUParticle* particleSystem, const nlohmann::json& json)
  {
    const Vector3 position = { json["position"][0], json["position"][1], json["position"][2] };
    const Vector3 v1 = { json["triangleV1"][0], json["triangleV1"][1], json["triangleV1"][2] };
    const Vector3 v2 = { json["triangleV2"][0], json["triangleV2"][1], json["triangleV2"][2] };
    const Vector3 v3 = { json["triangleV3"][0], json["triangleV3"][1], json["triangleV3"][2] };
    const uint32_t count = json["particleCount"];
    const float frequency = json["frequency"];
    return std::make_shared<TriangleEmitter>(particleSystem, position, v1, v2, v3, count, frequency);
  }

} // namespace Tako
