#pragma once
#include <d3d12.h>
#include<wrl.h>
#include "Matrix4x4.h"
#include "Light.h"

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
	Camera* GetCamera() const { return camera_; }
	bool GetDebug() const { return isDebug_; }

	// -----------------------------------Setters-----------------------------------//
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetDebug(bool isDebug) { isDebug_ = isDebug; }

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
	///ルートシグネチャの作成
	/// 	/// </summary>
	void CreateRootSignature();

	///<summary>
	///パイプラインステートの生成
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
