#include "Light.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include <numbers>

#include "Mat4x4Func.h"
#include "Camera.h"

void Light::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

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
	// 平行光源CBufferの場所を設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// ポイントライトsrvの場所を設定
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(5, pointLightSrvIndex_);

	// スポットライトCBufferの場所を設定
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(6, spotLightSrvIndex_);

	// ライト定数CBufferの場所を設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(7, lightConstantsResource_->GetGPUVirtualAddress());
}

void Light::SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity)
{
	directionalLightData_->direction = direction;
	directionalLightData_->color = color;
	directionalLightData_->lightType = lightType;
	directionalLightData_->intensity = intensity;
	
	// 自動更新が有効な場合、位置も更新
	if (autoUpdatePosition_) {
		// 方向が設定されたので、位置を自動計算するためのトリガーをリセット
		directionalLightData_->position = sceneCenter_;
	}
}

void Light::SetDirectionalLightDirection(const Vector3& direction)
{
	directionalLightData_->direction = direction;
	
	// 自動更新が有効な場合、位置も更新
	if (autoUpdatePosition_) {
		// 位置を自動計算するためのトリガーをリセット
		directionalLightData_->position = sceneCenter_;
	}
}

void Light::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index)
{
  // indexをチェック
  if (index >= Light::MAX_POINT_LIGHT)
  {
    return;
  }

  // indexの配列にこの値がないなら、新しい値を追加
  if (pointLightIndexList_.size() <= index && index <= Light::MAX_POINT_LIGHT)
  {
    pointLightIndexList_.resize(index + 1);
  }

	pointLightDatas_[index].position = position;
	pointLightDatas_[index].color = color;
	pointLightDatas_[index].intensity = intensity;
	pointLightDatas_[index].radius = radius;
	pointLightDatas_[index].decay = decay;
	pointLightDatas_[index].enable = enable;
}

void Light::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index)
{
  // indexをチェック
  if (index >= Light::MAX_SPOT_LIGHT)
  {
    return;
  }
  // indexの配列にこの値がないなら、新しい値を追加
  if (spotLightIndexList_.size() <= index && index <= Light::MAX_SPOT_LIGHT)
  {
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
	// 平行光源リソースを生成
	directionalLightResource_ = m_dx12_->MakeBufferResource(sizeof(DirectionalLight) * Light::MAX_DIRECTIONAL_LIGHT);

	// 平行光源リソースをマップ
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	// 平行光源データの初期値を書き込む
	directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f); // ライトの方向

	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色

	directionalLightData_->lightType = 1;                          // ライトのタイプ 0:Lambert 1:Half-Lambert

	directionalLightData_->intensity = 1.0f;                       // 輝度
	
	// シャドウマップ用の初期値設定
	directionalLightData_->position = Vector3(0.0f, 0.0f, 0.0f);   // 位置（0,0,0は自動計算のトリガー）
	directionalLightData_->viewMatrix = Mat4x4::MakeIdentity();
	directionalLightData_->projMatrix = Mat4x4::MakeIdentity();
	directionalLightData_->viewProjMatrix = Mat4x4::MakeIdentity();
}

void Light::CreatePointLightData()
{
	// 点光源リソースを生成
	pointLightResource_ = m_dx12_->MakeBufferResource(sizeof(PointLight) * Light::MAX_POINT_LIGHT);

	// 点光源リソースをマップ
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightDatas_));

	// 点光源データの初期値を書き込む
	pointLightDatas_[0].position = Vector3(0.0f, 2.0f, 0.0f); // ライトの位置
	pointLightDatas_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	pointLightDatas_[0].intensity = 1.0f;                       // 輝度
	pointLightDatas_[0].radius = 10.0f;                         // 半径
	pointLightDatas_[0].decay = 1.0f;                           // 減衰
	pointLightDatas_[0].enable = false;                         // 点光源の有効無効

	// SRVの生成
	pointLightSrvIndex_ = SrvManager::GetInstance()->Allocate();
	SrvManager::GetInstance()->CreateSRVForStructuredBuffer(pointLightSrvIndex_, pointLightResource_.Get(), Light::MAX_POINT_LIGHT, sizeof(PointLight));
}

void Light::CreateSpotLightData()
{
	// スポットライトリソースを生成
	spotLightResource_ = m_dx12_->MakeBufferResource(sizeof(SpotLight) * Light::MAX_SPOT_LIGHT);

	// スポットライトリソースをマップ
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

	// スポットライトデータの初期値を書き込む
	spotLightData_[0].position = Vector3(0.0f, 0.0f, 0.0f); // ライトの位置
	spotLightData_[0].direction = Vector3(-1.0f, -1.0f, 0.0f); // ライトの方向
	spotLightData_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	spotLightData_[0].intensity = 1.0f;                       // 輝度
	spotLightData_[0].distance = 10.0f;                        // 距離
	spotLightData_[0].decay = 1.0f;                           // 減衰
	spotLightData_[0].cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f); // 角度
	spotLightData_[0].enable = true;                         // スポットライトの有効無効

	// SRVの生成
	spotLightSrvIndex_ = SrvManager::GetInstance()->Allocate();
	SrvManager::GetInstance()->CreateSRVForStructuredBuffer(spotLightSrvIndex_, spotLightResource_.Get(), Light::MAX_SPOT_LIGHT, sizeof(SpotLight));
}

void Light::CreateLightConstants()
{
	// ライト定数リソースを生成
	lightConstantsResource_ = m_dx12_->MakeBufferResource(sizeof(LightConsteants));

	// ライト定数リソースをマップ
	lightConstantsResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightConstantsData_));

	// ライト定数データの初期値を書き込む
	lightConstantsData_->numPointLights = 0;

	lightConstantsData_->numSpotLights = 0;
}

void Light::UpdateDirectionalLightShadowMatrices(const Camera* camera, float maxShadowDistance)
{
	if (!camera) {
		return; // カメラが指定されていない場合は処理しない
	}
	
	Vector3 lightDirection = directionalLightData_->direction;
	lightDirection = lightDirection.Normalize();
	
	// ライトの仮の位置を設定（カメラ位置を基準に）
	Vector3 cameraPosition = camera->GetTranslate();
	float tempDistance = 100.0f; // ライトカメラの仮の距離
	Vector3 lightPosition = cameraPosition - lightDirection * tempDistance;
	
	// ライトのビュー行列を作成
	Vector3 scale = {1.0f, 1.0f, 1.0f};
	Vector3 rotate = {0.0f, 0.0f, 0.0f};
	
	// Y軸まわりの回転角度を計算（XZ平面での方向から）
	rotate.y = std::atan2f(lightDirection.x, lightDirection.z);
	
	// X軸まわりの回転角度を計算（ピッチ角）
	float horizontalLength = std::sqrtf(lightDirection.x * lightDirection.x + lightDirection.z * lightDirection.z);
	rotate.x = std::atan2f(-lightDirection.y, horizontalLength);
	
	// ライトのワールド行列を作成
	Matrix4x4 lightWorldMatrix = Mat4x4::MakeAffine(scale, rotate, lightPosition);
	
	// ビュー行列はワールド行列の逆行列
	directionalLightData_->viewMatrix = Mat4x4::Inverse(lightWorldMatrix);
	
	// maxShadowDistanceで制限された視錐台の境界ボックスをライト空間で取得
	auto [minBounds, maxBounds] = camera->GetFrustumBoundingBoxWithCustomFar(maxShadowDistance, &directionalLightData_->viewMatrix);
	
	// 視錐台を制限された範囲でカバーする正射影パラメータを計算
	float orthoLeft = minBounds.x;
	float orthoRight = maxBounds.x;
	float orthoBottom = minBounds.y;
	float orthoTop = maxBounds.y;
	float orthoNear = minBounds.z - 20.0f; // 余裕を持たせる（最も近い点から少し手前）
	float orthoFar = maxBounds.z + 20.0f;  // 余裕を持たせる（最も遠い点から少し奥）
	
	// 安全性チェック（near/farが逆転しないように）
	if (orthoNear >= orthoFar) {
		orthoNear = 0.1f;
		orthoFar = 1000.0f;
	}
	
	// 正射影行列の作成
	directionalLightData_->projMatrix = Mat4x4::MakeOrtho(
		orthoLeft, orthoTop,
		orthoRight, orthoBottom,
		orthoNear, orthoFar);
	
	// ビュープロジェクション行列の計算
	directionalLightData_->viewProjMatrix = Mat4x4::Multiply(directionalLightData_->viewMatrix, directionalLightData_->projMatrix);
	
	// ライト位置を保存（互換性のため）
	if (autoUpdatePosition_) {
		directionalLightData_->position = lightPosition;
	}
}
