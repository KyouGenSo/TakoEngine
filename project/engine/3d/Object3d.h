#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"

class Model;

class Camera;

class Object3d {

public: // 構造体

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 world;
		Matrix4x4 worldInvTranspose;
	};

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
		//float padding[2];
		bool enable;
	};

	// Shader用のカメラ
	struct CameraForGPU
	{
		Vector3 worldPos;
	};


public: // メンバー関数
	///<summary>
	///初期化
	/// </summary>
	void Initialize();

	///<summary>
	///更新
	/// </summary>
	void Update();

	///<summary>
	///描画
	/// </summary>
	void Draw();

	//-----------------------------------------Getter-----------------------------------------//
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }

	//-----------------------------------------Setter-----------------------------------------//
	void SetModel(const std::string& fileName);
	void SetCamera(Camera* camera) { m_camera_ = camera; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	// DirectionalLight
	void SetShininess(float shininess);
	void SetEnableLighting(bool enableLighting);
	void SetEnableHighlight(bool enableHighlight);
	void SetLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }
	void SetLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetLightType(int32_t lightType) { directionalLightData_->lightType = lightType; }
	void SetLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

	// PointLight
	void SetPointLightColor(const Vector4& color) { pointLightData_->color = color; }
	void SetPointLightPosition(const Vector3& position) { pointLightData_->position = position; }
	void SetPointLightIntensity(float intensity) { pointLightData_->intensity = intensity; }
	void SetPointLightRadius(float radius) { pointLightData_->radius = radius; }
	void SetPointLightDecay(float decay) { pointLightData_->decay = decay; }
	void SetPointLightEnable(bool enable) { pointLightData_->enable = enable; }

	// SpotLight
	void SetSpotLightColor(const Vector4& color) { spotLightData_->color = color; }
	void SetSpotLightPosition(const Vector3& position) { spotLightData_->position = position; }
	void SetSpotLightIntensity(float intensity) { spotLightData_->intensity = intensity; }
	void SetSpotLightDirection(const Vector3& direction) { spotLightData_->direction = direction; }
	void SetSpotLightDistance(float distance) { spotLightData_->distance = distance; }
	void SetSpotLightDecay(float decay) { spotLightData_->decay = decay; }
	void SetSpotLightCosAngle(float cosAngle) { spotLightData_->cosAngle = cosAngle; }
	void SetSpotLightEnable(bool enable) { spotLightData_->enable = enable; }

private: // プライベートメンバー関数

	///<summary>
	///　座標変換行列データの生成
	/// </summary>
	void CreateTransformationMatrixData();

	///<summary>
	///　平行光源データの生成
	/// </summary>
	void CreateDirectionalLightData();

	///<summary>
	///　点光源データの生成
	/// </summary>
	void CreatePointLightData();

	/// <summary>
	/// シェーダー用カメラデータの生成
	/// </summary>
	void CreateCameraForGPUData();

	///<summary>
	///　スポットライトデータの生成
	///</summary>
	void CreateSpotLightData();

private: // メンバー変数

	// カメラのクラスポインター
	Camera* m_camera_ = nullptr;

	// モデルクラス
	Model* m_model_ = nullptr;

	// トランスフォーム
	Transform transform_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatData_ = nullptr;
	DirectionalLight* directionalLightData_ = nullptr;
	PointLight* pointLightData_ = nullptr;
	SpotLight* spotLightData_ = nullptr;
	CameraForGPU* cameraForGPUData_ = nullptr;

};
