#include "Light.h"
#include "DX12Basic.h"
#include "SrvManager.h"

#include "ColorFunc.h"

namespace Tako {

  Light::~Light()
  {
    SrvManager::GetInstance()->Free(pointLightSrvIndex_);
    SrvManager::GetInstance()->Free(spotLightSrvIndex_);
  }

  void Light::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    CreateDirectionalLightData();
    CreatePointLightData();
    CreateSpotLightData();
    CreateLightConstants();
  }

  void Light::BeginFrame()
  {
    lightConstantsData_->numPointLights = 0;
    lightConstantsData_->numSpotLights  = 0;
    shadowRequests_.clear();
  }

  void Light::AddPointLight(const PointLight& light, bool castShadow)
  {
    int& count = lightConstantsData_->numPointLights;
    if (count < MAX_POINT_LIGHT) {
      pointLightData_[count] = light;
      pointLightData_[count].color = light.color.ToLinear();
      pointLightData_[count].enable = true;
      pointLightData_[count].shadowSlice = -1;
      if (castShadow) {
        shadowRequests_.push_back({ .isSpot = false, .lightIndex = static_cast<uint32_t>(count), .position = light.position, .range = light.radius });
      }
      ++count;
    }
  }

  void Light::AddSpotLight(const SpotLight& light, bool castShadow)
  {
    int& count = lightConstantsData_->numSpotLights;
    if (count < MAX_SPOT_LIGHT) {
      spotLightData_[count] = light;
      spotLightData_[count].color = light.color.ToLinear();
      spotLightData_[count].enable = true;
      spotLightData_[count].shadowSlice = -1;
      if (castShadow) {
        shadowRequests_.push_back({ .isSpot     = true,
                                    .lightIndex = static_cast<uint32_t>(count),
                                    .position   = light.position,
                                    .direction  = light.direction,
                                    .range      = light.distance,
                                    .cosAngle   = light.cosAngle });
      }
      ++count;
    }
  }

  void Light::SetShadowSlice(const ShadowRequest& request, int32_t slice)
  {
    if (request.isSpot) {
      spotLightData_[request.lightIndex].shadowSlice = slice;
    }
    else {
      pointLightData_[request.lightIndex].shadowSlice = slice;
    }
  }

  void Light::ResetDirectionalLight()
  {
    directionalLightData_->direction      = Vector3(0.0f, -1.0f, 0.0f);
    directionalLightData_->color          = Color::kWhite;
    directionalLightData_->lightType      = 1;  // 0:Lambert 1:Half-Lambert
    directionalLightData_->intensity      = 1.0f;
  }

  void Light::PreDraw() const
  {
    CommandContext::ShaderBinder binder = dx12_->GetContext().Graphics();
    binder.BindCBV("gDirectionalLight"_id, directionalLightResource_->GetGPUVirtualAddress());
    binder.BindSRV("gPointLights"_id, pointLightSrvIndex_);
    binder.BindSRV("gSpotLight"_id, spotLightSrvIndex_);
    binder.BindCBV("LightConstants"_id, lightConstantsResource_->GetGPUVirtualAddress());
  }

  void Light::SetDirectionalLight(const Vector3& direction, const Color& color, int32_t lightType, float intensity)
  {
    directionalLightData_->direction = direction;
    directionalLightData_->color = color.ToLinear();
    directionalLightData_->lightType = lightType;
    directionalLightData_->intensity = intensity;
  }

  void Light::CreateDirectionalLightData()
  {
    directionalLightResource_ = dx12_->MakeBufferResource(sizeof(DirectionalLight) * Light::MAX_DIRECTIONAL_LIGHT);

    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

    ResetDirectionalLight();
  }

  void Light::CreatePointLightData()
  {
    pointLightResource_ = dx12_->MakeBufferResource(sizeof(PointLight) * Light::MAX_POINT_LIGHT);

    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

    pointLightSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForStructuredBuffer(pointLightSrvIndex_, pointLightResource_.Get(), Light::MAX_POINT_LIGHT, sizeof(PointLight));
  }

  void Light::CreateSpotLightData()
  {
    spotLightResource_ = dx12_->MakeBufferResource(sizeof(SpotLight) * Light::MAX_SPOT_LIGHT);

    spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

    spotLightSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForStructuredBuffer(spotLightSrvIndex_, spotLightResource_.Get(), Light::MAX_SPOT_LIGHT, sizeof(SpotLight));
  }

  void Light::CreateLightConstants()
  {
    lightConstantsResource_ = dx12_->MakeBufferResource(sizeof(LightConstants));

    lightConstantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightConstantsData_));

    lightConstantsData_->numPointLights = 0;

    lightConstantsData_->numSpotLights = 0;
  }

} // namespace Tako
