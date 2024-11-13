#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"

class Light
{

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

public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// ImGuiの描画
	/// </summary>
	void DrawImGui();

//	/// <summary>
//	/// 平行光源データの取得
//	/// </summary>
//	/// <returns>平行光源データ</returns>
//	DirectionalLight* GetDirectionalLightData() { return &directionalLightData_; }
//
//	/// <summary>
//	/// 点光源データの取得
//	/// </summary>
//	/// <returns>点光源データ</returns>
//	PointLight* GetPointLightData() { return &pointLightData_; }
//
//	/// <summary>
//	/// スポットライトデータの取得
//	/// </summary>
//	/// <returns>スポットライトデータ</returns>
//	SpotLight* GetSpotLightData() { return &spotLightData_; }
//
//private: // メンバ変数
//
//	// 平行光源データ
//	DirectionalLight directionalLightData_;
//
//	// 点光源データ
//	PointLight pointLightData_;
//
//	// スポットライトデータ
//	SpotLight spotLightData_;
//
//	// 平行光源リソース
//	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
//
//	// 点光源リソース
//	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
//
//	// スポットライトリソース
//	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
//
//	// カメラリソース
//	Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;
//
//	// カメラデータ
//	struct CameraForGPU
//	{
//		Vector3 worldPos;
//	};
//
//	// カメラデータ
//	CameraForGPU* cameraForGPUData_;
};