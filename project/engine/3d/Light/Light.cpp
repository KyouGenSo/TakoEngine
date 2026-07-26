#include "Light.h"
#include "DX12Basic.h"
#include "Object3dBasic.h"
#include "SrvManager.h"
#include <numbers>

#include "Mat4x4Func.h"
#include "Camera.h"

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

  void Light::Update()
  {
    lightConstantsData_->numPointLights = static_cast<int>(pointLightIndexList_.size());
    lightConstantsData_->numSpotLights = static_cast<int>(spotLightIndexList_.size());
  }

  void Light::PreDraw()
  {
    // 平行光源 CBV (b1)
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(Object3dBasic::kDirectionalLightParam, directionalLightResource_->GetGPUVirtualAddress());

    // 点光源 SRV (t1)
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kPointLightParam, pointLightSrvIndex_);

    // スポットライト SRV (t2)
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(Object3dBasic::kSpotLightParam, spotLightSrvIndex_);

    // ライト定数 CBV (b3)
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(Object3dBasic::kLightConstantsParam, lightConstantsResource_->GetGPUVirtualAddress());
  }

  void Light::SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity)
  {
    directionalLightData_->direction = direction;
    directionalLightData_->color = color;
    directionalLightData_->lightType = lightType;
    directionalLightData_->intensity = intensity;

    if (autoUpdatePosition_) {
      directionalLightData_->position = sceneCenter_;
    }
  }

  void Light::SetDirectionalLightDirection(const Vector3& direction)
  {
    directionalLightData_->direction = direction;

    if (autoUpdatePosition_) {
      directionalLightData_->position = sceneCenter_;
    }
  }

  void Light::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index)
  {
    if (index >= Light::MAX_POINT_LIGHT) {
      return;
    }

    // index が未使用ならリストを伸ばして点灯数に反映
    if (pointLightIndexList_.size() <= index && index <= Light::MAX_POINT_LIGHT) {
      pointLightIndexList_.resize(index + 1);
    }

    pointLightData_[index].position = position;
    pointLightData_[index].color = color;
    pointLightData_[index].intensity = intensity;
    pointLightData_[index].radius = radius;
    pointLightData_[index].decay = decay;
    pointLightData_[index].enable = enable;
  }

  void Light::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index)
  {
    if (index >= Light::MAX_SPOT_LIGHT) {
      return;
    }
    // index が未使用ならリストを伸ばして点灯数に反映
    if (spotLightIndexList_.size() <= index && index <= Light::MAX_SPOT_LIGHT) {
      spotLightIndexList_.resize(index + 1);
    }

    spotLightData_[index].color = color;
    spotLightData_[index].position = position;
    spotLightData_[index].intensity = intensity;
    spotLightData_[index].direction = direction;
    spotLightData_[index].distance = distance;
    spotLightData_[index].decay = decay;
    spotLightData_[index].cosAngle = cosAngle;
    spotLightData_[index].enable = enable;
  }

  void Light::CreateDirectionalLightData()
  {
    directionalLightResource_ = dx12_->MakeBufferResource(sizeof(DirectionalLight) * Light::MAX_DIRECTIONAL_LIGHT);

    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

    directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f);

    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };

    directionalLightData_->lightType = 1;                          // 0:Lambert 1:Half-Lambert

    directionalLightData_->intensity = 1.0f;

    directionalLightData_->position = Vector3(0.0f, 0.0f, 0.0f);
    directionalLightData_->viewMatrix = Mat4x4::MakeIdentity();
    directionalLightData_->projMatrix = Mat4x4::MakeIdentity();
    directionalLightData_->viewProjMatrix = Mat4x4::MakeIdentity();
  }

  void Light::CreatePointLightData()
  {
    pointLightResource_ = dx12_->MakeBufferResource(sizeof(PointLight) * Light::MAX_POINT_LIGHT);

    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

    pointLightData_[0].position = Vector3(0.0f, 2.0f, 0.0f);
    pointLightData_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLightData_[0].intensity = 1.0f;
    pointLightData_[0].radius = 10.0f;
    pointLightData_[0].decay = 1.0f;
    pointLightData_[0].enable = false;

    pointLightSrvIndex_ = SrvManager::GetInstance()->Allocate();
    SrvManager::GetInstance()->CreateSRVForStructuredBuffer(pointLightSrvIndex_, pointLightResource_.Get(), Light::MAX_POINT_LIGHT, sizeof(PointLight));
  }

  void Light::CreateSpotLightData()
  {
    spotLightResource_ = dx12_->MakeBufferResource(sizeof(SpotLight) * Light::MAX_SPOT_LIGHT);

    spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

    spotLightData_[0].position = Vector3(0.0f, 0.0f, 0.0f);
    spotLightData_[0].direction = Vector3(-1.0f, -1.0f, 0.0f);
    spotLightData_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
    spotLightData_[0].intensity = 1.0f;
    spotLightData_[0].distance = 10.0f;
    spotLightData_[0].decay = 1.0f;
    spotLightData_[0].cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f); // コーン半角60度
    spotLightData_[0].enable = false;

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

  void Light::UpdateDirectionalLightShadowMatrices(const Camera* camera, float maxShadowDistance)
  {
    if (!camera) {
      return;
    }

    Vector3 lightDirection = directionalLightData_->direction;
    lightDirection = lightDirection.Normalize();

    // カメラから光方向の逆へ離した位置にライトカメラを置く
    Vector3 cameraPosition = camera->GetTranslate();
    float tempDistance = 100.0f;
    Vector3 lightPosition = cameraPosition - lightDirection * tempDistance;

    // ライトのビュー行列を作成
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Vector3 rotate = { 0.0f, 0.0f, 0.0f };

    // Y 軸まわりの回転角度を計算（XZ 平面での方向から）
    rotate.y = std::atan2f(lightDirection.x, lightDirection.z);

    // X 軸まわりの回転角度を計算（ピッチ角）
    float horizontalLength = std::sqrtf(lightDirection.x * lightDirection.x + lightDirection.z * lightDirection.z);
    rotate.x = std::atan2f(-lightDirection.y, horizontalLength);

    // ライトのワールド行列を作成
    Matrix4x4 lightWorldMatrix = Mat4x4::MakeAffine(scale, rotate, lightPosition);

    // ビュー行列はワールド行列の逆行列
    directionalLightData_->viewMatrix = Mat4x4::Inverse(lightWorldMatrix);

    // maxShadowDistance で制限された視錐台の境界ボックスをライト空間で取得
    auto [minBounds, maxBounds] = camera->GetFrustumBoundingBoxWithCustomFar(maxShadowDistance, &directionalLightData_->viewMatrix);

    // 視錐台を制限された範囲でカバーする正射影パラメータを計算
    float orthoLeft = minBounds.x;
    float orthoRight = maxBounds.x;
    float orthoBottom = minBounds.y;
    float orthoTop = maxBounds.y;
    float orthoNear = minBounds.z - 20.0f; // 余裕を持たせる（最も近い点から少し手前）
    float orthoFar = maxBounds.z + 20.0f;  // 余裕を持たせる（最も遠い点から少し奥）

    // 安全性チェック（near/far が逆転しないように）
    if (orthoNear >= orthoFar) {
      orthoNear = 0.1f;
      orthoFar = 1000.0f;
    }

    // 正射影行列の作成
    directionalLightData_->projMatrix = Mat4x4::MakeOrtho(
      orthoLeft, orthoTop,
      orthoRight, orthoBottom,
      orthoNear, orthoFar);

    directionalLightData_->viewProjMatrix = Mat4x4::Multiply(directionalLightData_->viewMatrix, directionalLightData_->projMatrix);

    if (autoUpdatePosition_) {
      directionalLightData_->position = lightPosition;
    }
  }

} // namespace Tako
