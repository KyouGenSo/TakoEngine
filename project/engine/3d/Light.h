#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"

class DX12Basic;

class Light
{
public: // 定数
	// ライトの最大数
	static const int32_t MAX_DIRECTIONAL_LIGHT = 1;
	static const int32_t MAX_POINT_LIGHT = 256;
	static const int32_t MAX_SPOT_LIGHT = 1;

public: // 構造体
	// 平行光源データ
	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		int32_t lightType;
		float intensity;
	};

	// 点光源データ
	struct PointLight
	{
		Vector4 color;
		Vector3 position;
		float intensity;
		float radius;
		float decay;
		bool enable;
	};

	// スポットライトデータ
	struct SpotLight
	{
		Vector4 color;
		Vector3 position;
		float intensity;
		Vector3 direction;
		float distance;
		float decay;
		float cosAngle;
		bool enable;
	};

	struct LightConsteants
	{
		int numPointLights;
		float padding[3];
	};

public: // メンバ関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	/// <summary>
	/// 描画設定
	/// </summary>
	void PreDraw();

	//-----------------------------------------Setter-----------------------------------------//
	// DirectionalLight
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);
	void SetDirectionalLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }
	void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetDirectionalLightType(int32_t lightType) { directionalLightData_->lightType = lightType; }
	void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

	// PointLight
	void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);
	void SetPointLightColor(const Vector4& color) { pointLightDatas_->color = color; }
	void SetPointLightPosition(const Vector3& position) { pointLightDatas_->position = position; }
	void SetPointLightIntensity(float intensity) { pointLightDatas_->intensity = intensity; }
	void SetPointLightRadius(float radius) { pointLightDatas_->radius = radius; }
	void SetPointLightDecay(float decay) { pointLightDatas_->decay = decay; }
	void SetPointLightEnable(bool enable) { pointLightDatas_->enable = enable; }

	// SpotLight
	void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable);
	void SetSpotLightColor(const Vector4& color) { spotLightData_->color = color; }
	void SetSpotLightPosition(const Vector3& position) { spotLightData_->position = position; }
	void SetSpotLightIntensity(float intensity) { spotLightData_->intensity = intensity; }
	void SetSpotLightDirection(const Vector3& direction) { spotLightData_->direction = direction; }
	void SetSpotLightDistance(float distance) { spotLightData_->distance = distance; }
	void SetSpotLightDecay(float decay) { spotLightData_->decay = decay; }
	void SetSpotLightCosAngle(float cosAngle) { spotLightData_->cosAngle = cosAngle; }
	void SetSpotLightEnable(bool enable) { spotLightData_->enable = enable; }

private: // プライベートメンバ関数
	///<summary>
	///　平行光源データの生成
	/// </summary>
	void CreateDirectionalLightData();

	///<summary>
	///　点光源データの生成
	/// </summary>
	void CreatePointLightData();

	///<summary>
	///　スポットライトデータの生成
	///</summary>
	void CreateSpotLightData();

	/// <summary>
	/// LightConstantsの生成
	/// </summary>
	void CreateLightConstants();

private: // メンバ変数
	// DX12
	DX12Basic* m_dx12_;

	// 平行光源データ
	DirectionalLight* directionalLightData_;

	// 点光源データ
	PointLight* pointLightDatas_;

	// スポットライトデータ
	SpotLight* spotLightData_;

	// LightConstants
	LightConsteants* lightConstantsData_;

	// 平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;

	// 点光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;

	// スポットライトリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;

	// LightConstantsリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> lightConstantsResource_;

	// 点光源のsrvIndex
	int pointLightSrvIndex_;
};