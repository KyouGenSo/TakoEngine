#pragma once
#include <d3d12.h>
#include<wrl.h>

#include "Camera.h"
#include "Matrix4x4.h"
#include "Light.h"
#include "SkyBox.h"

class DX12Basic;

class Camera;

class Object3dBasic {
private: // シングルトン設定

	// インスタンス
	static Object3dBasic* instance_;

	Object3dBasic() = default;
	~Object3dBasic() = default;
	Object3dBasic(Object3dBasic&) = delete;
	Object3dBasic& operator=(Object3dBasic&) = delete;

public: // メンバー関数

	///<summary>
	///　インスタンスの取得
	///	</summary>
	static Object3dBasic* GetInstance();

	///<summary>
	///　初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	///<summary>
	///　更新
	/// </summary>
	void Update();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	///<summary>
	///　共通描画設定
	/// </summary>
	void SetCommonRenderSetting();

	// -----------------------------------Getters-----------------------------------//
	DX12Basic* GetDX12Basic() const { return m_dx12_; }
	Camera** GetCamera() { return &camera_; }
	bool GetDebug() const { return isDebug_; }

	// -----------------------------------Setters-----------------------------------//
	void SetCamera(Camera* camera) { camera_ = camera; }
  void SetCameraTranslate(Vector3 translate) { camera_->SetTranslate(translate); }
  void SetCameraRotation(Vector3 rotation) { camera_->SetRotate(rotation); }
	void SetDebug(bool isDebug) { isDebug_ = isDebug; }

	// DirectionalLight
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);
	void SetDirectionalLightDirection(const Vector3& direction) { light_->SetDirectionalLightDirection(direction); }
	void SetDirectionalLightColor(const Vector4& color) { light_->SetDirectionalLightColor(color); }
	void SetDirectionalLightType(int32_t lightType) { light_->SetDirectionalLightType(lightType); }
	void SetDirectionalLightIntensity(float intensity) { light_->SetDirectionalLightIntensity(intensity); }

	// PointLight
	void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);
  void SetPointLightColor(const Vector4& color, int index) { light_->SetPointLightColor(color, index); }
  void SetPointLightPos(const Vector3& position, int index) { light_->SetPointLightPos(position, index); }
  void SetPointLightIntensity(float intensity, int index) { light_->SetPointLightIntensity(intensity, index); }
  void SetPointLightRadius(float radius, int index) { light_->SetPointLightRadius(radius, index); }
  void SetPointLightDecay(float decay, int index) { light_->SetPointLightDecay(decay, index); }
  void SetPointLightEnable(bool enable, int index) { light_->SetPointLightEnable(enable, index); }

	// SpotLight
	void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index);
  void SetSpotLightColor(const Vector4& color, int index) { light_->SetSpotLightColor(color, index); }
  void SetSpotLightPos(const Vector3& position, int index) { light_->SetSpotLightPos(position, index); }
  void SetSpotLightIntensity(float intensity, int index) { light_->SetSpotLightIntensity(intensity, index); }
  void SetSpotLightDistance(float distance, int index) { light_->SetSpotLightDistance(distance, index); }
  void SetSpotLightDecay(float decay, int index) { light_->SetSpotLightDecay(decay, index); }
  void SetSpotLightCosAngle(float cosAngle, int index) { light_->SetSpotLightCosAngle(cosAngle, index); }
  void SetSpotLightEnable(bool enable, int index) { light_->SetSpotLightEnable(enable, index); }

private: // プライベートメンバー関数

	///<summary>
	/// ルートシグネチャの作成
	/// 	/// </summary>
	void CreateRootSignature();

	///<summary>
	/// パイプラインステートの生成
	/// </summary>
	void CreatePSO();

private: // メンバー変数
	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	// デフォルトカメラ
	Camera* camera_ = nullptr;

	// ライトクラス
	Light* light_ = nullptr;

	// ビュープロジェクション行列
	Matrix4x4 viewProjectionMatrix_;
	Matrix4x4 debugViewProjectionMatrix_;

	// デバッグフラグ
	bool isDebug_ = false;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
