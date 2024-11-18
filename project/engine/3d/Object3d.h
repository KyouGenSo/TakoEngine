#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <string>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Mat4x4Func.h"
#include "Light.h"

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
		bool enable;
	};

	// Shader用のカメラ
	struct CameraForGPU
	{
		Vector3 worldPos;
	};


public: // メンバー関数
	///<summary>
	/// デストラクタ
	/// </summary>
	~Object3d();

	///<summary>
	/// 初期化
	/// </summary>
	void Initialize();

	///<summary>
	/// 更新
	/// </summary>
	void Update();

	///<summary>
	/// 描画
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

	// ライト全体の設定
	void SetShininess(float shininess);
	void SetEnableLighting(bool enableLighting);
	void SetEnableHighlight(bool enableHighlight);

	// DirectionalLight
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);
	void SetDirectionalLightDirection(const Vector3& direction) { light_->SetDirectionalLightDirection(direction); }
	void SetDirectionalLightColor(const Vector4& color) { light_->SetDirectionalLightColor(color); }
	void SetDirectionalLightType(int32_t lightType) { light_->SetDirectionalLightType(lightType); }
	void SetDirectionalLightIntensity(float intensity) { light_->SetDirectionalLightIntensity(intensity); }

	// PointLight
	void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);
	void SetPointLightColor(const Vector4& color) { light_->SetPointLightColor(color); }
	void SetPointLightPosition(const Vector3& position) { light_->SetPointLightPosition(position); }
	void SetPointLightIntensity(float intensity) { light_->SetPointLightIntensity(intensity); }
	void SetPointLightRadius(float radius) { light_->SetPointLightRadius(radius); }
	void SetPointLightDecay(float decay) { light_->SetPointLightDecay(decay); }
	void SetPointLightEnable(bool enable) { light_->SetPointLightEnable(enable); }

	// SpotLight
	void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable);
	void SetSpotLightColor(const Vector4& color) { light_->SetSpotLightColor(color); }
	void SetSpotLightPosition(const Vector3& position) { light_->SetSpotLightPosition(position); }
	void SetSpotLightIntensity(float intensity) { light_->SetSpotLightIntensity(intensity); }
	void SetSpotLightDirection(const Vector3& direction) { light_->SetSpotLightDirection(direction); }
	void SetSpotLightDistance(float distance) { light_->SetSpotLightDistance(distance); }
	void SetSpotLightDecay(float decay) { light_->SetSpotLightDecay(decay); }
	void SetSpotLightCosAngle(float cosAngle) { light_->SetSpotLightCosAngle(cosAngle); }
	void SetSpotLightEnable(bool enable) { light_->SetSpotLightEnable(enable); }

private: // プライベートメンバー関数
	///<summary>
	///　座標変換行列データの生成
	/// </summary>
	void CreateTransformationMatrixData();

	/// <summary>
	/// シェーダー用カメラデータの生成
	/// </summary>
	void CreateCameraForGPUData();

private: // メンバー変数
	// カメラのクラスポインター
	Camera* m_camera_ = nullptr;

	// モデルクラス
	Model* m_model_ = nullptr;

	// ライトクラス
	Light* light_ = nullptr;

	// トランスフォーム
	Transform transform_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;

	// バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatData_ = nullptr;
	CameraForGPU* cameraForGPUData_ = nullptr;
};
