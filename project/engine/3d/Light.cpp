#include "Light.h"
#include "DX12Basic.h"
#include <numbers>

void Light::Initialize(DX12Basic* dx12)
{
	m_dx12_ = dx12;

	CreateDirectionalLightData();
	CreatePointLightData();
	CreateSpotLightData();
}

void Light::PreDraw()
{
	// 平行光源CBufferの場所を設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// ポイントライトCBufferの場所を設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());

	// スポットライトCBufferの場所を設定
	m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
}

void Light::SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity)
{
	directionalLightData_->direction = direction;
	directionalLightData_->color = color;
	directionalLightData_->lightType = lightType;
	directionalLightData_->intensity = intensity;
}

void Light::SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable)
{
	pointLightDatas_->position = position;
	pointLightDatas_->color = color;
	pointLightDatas_->intensity = intensity;
	pointLightDatas_->radius = radius;
	pointLightDatas_->decay = decay;
	pointLightDatas_->enable = enable;
}

void Light::SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable)
{
	spotLightData_->position = position;
	spotLightData_->direction = direction;
	spotLightData_->color = color;
	spotLightData_->intensity = intensity;
	spotLightData_->distance = distance;
	spotLightData_->decay = decay;
	spotLightData_->cosAngle = cosAngle;
	spotLightData_->enable = enable;
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
}

void Light::CreatePointLightData()
{
	// 点光源リソースを生成
	pointLightResource_ = m_dx12_->MakeBufferResource(sizeof(PointLight) * Light::MAX_POINT_LIGHT);

	// 点光源リソースをマップ
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightDatas_));

	// 点光源データの初期値を書き込む
	pointLightDatas_->position = Vector3(0.0f, 2.0f, 0.0f); // ライトの位置
	pointLightDatas_->color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	pointLightDatas_->intensity = 1.0f;                       // 輝度
	pointLightDatas_->radius = 10.0f;                         // 半径
	pointLightDatas_->decay = 1.0f;                           // 減衰
	pointLightDatas_->enable = true;                         // 点光源の有効無効
}

void Light::CreateSpotLightData()
{
	// スポットライトリソースを生成
	spotLightResource_ = m_dx12_->MakeBufferResource(sizeof(SpotLight) * Light::MAX_SPOT_LIGHT);

	// スポットライトリソースをマップ
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

	// スポットライトデータの初期値を書き込む
	spotLightData_->position = Vector3(0.0f, 0.0f, 0.0f); // ライトの位置
	spotLightData_->direction = Vector3(-1.0f, -1.0f, 0.0f); // ライトの方向
	spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };     // ライトの色
	spotLightData_->intensity = 1.0f;                       // 輝度
	spotLightData_->distance = 10.0f;                        // 距離
	spotLightData_->decay = 1.0f;                           // 減衰
	spotLightData_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f); // 角度
	spotLightData_->enable = true;                         // スポットライトの有効無効
}
