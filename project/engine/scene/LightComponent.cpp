#include "LightComponent.h"
#include "Actor.h"
#include "Light.h"
#include "Mat4x4Func.h"
#include "Renderer.h"
#include "ShadowRenderer.h"

#include <cmath>

namespace Tako {

  LightComponent::LightComponent()
    : shadowDistance_(ShadowRenderer::kDefaultMaxShadowDistance)
  {
  }

  void LightComponent::Draw()
  {
    const Vector3 direction = Mat4x4::TransformNormal(GetOwner()->GetWorldMatrix(), Vector3(0.0f, 0.0f, 1.0f)).Normalize();
    const Vector3 position  = GetOwner()->GetWorldPosition();
    Light*        light     = Renderer::GetInstance()->GetLight();
    switch (type_) {
      case Type::Directional:
        light->SetDirectionalLight(direction, color_, light->GetDirectionalLight().lightType, intensity_);
        ShadowRenderer::GetInstance()->SetMaxShadowDistance(shadowDistance_);
        break;
      case Type::Point:
        light->AddPointLight({ .color = color_, .position = position, .intensity = intensity_, .radius = range_, .decay = decay_ });
        break;
      case Type::Spot:
        light->AddSpotLight({ .color     = color_,
                              .position  = position,
                              .intensity = intensity_,
                              .direction = direction,
                              .distance  = range_,
                              .decay     = decay_,
                              .cosAngle  = std::cos(spotAngle_) });
        break;
    }
  }

} // namespace Tako
