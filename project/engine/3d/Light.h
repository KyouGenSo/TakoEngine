#pragma once
#include <d3d12.h>
#include<wrl.h>
#include <vector>
#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"

class DX12Basic;

class Light
{
public: // 定数
	// ライトの最大数
	static const int32_t MAX_DIRECTIONAL_LIGHT = 1;
	static const int32_t MAX_POINT_LIGHT = 256;
	static const int32_t MAX_SPOT_LIGHT = 256;

public: // 構造体
	// 平行光源データ
	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		int32_t lightType;
		float intensity;
		Matrix4x4 viewMatrix;
		Matrix4x4 projMatrix;
		Matrix4x4 viewProjMatrix;
		Vector3 position;  // ライトの位置（シャドウマップ用）
		float shadowDistance;  // シャドウの範囲
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
		int numSpotLights;
		int pad1;
		int pad2;
	};

public: // メンバ関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

  /// <summary>
  /// 更新処理
  /// </summary>
  void Update();

	/// <summary>
	/// 描画設定
	/// </summary>
	void PreDraw();

	//-----------------------------------------Setter-----------------------------------------//
	// DirectionalLight
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);
	void SetDirectionalLightDirection(const Vector3& direction);
	void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetDirectionalLightType(int32_t lightType) { directionalLightData_->lightType = lightType; }
	void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }
	void SetDirectionalLightPosition(const Vector3& position) { directionalLightData_->position = position; }
	void SetDirectionalLightShadowDistance(float distance) { directionalLightData_->shadowDistance = distance; }
	
	// シャドウマップ用の行列計算
	void UpdateDirectionalLightShadowMatrices();
	
	// ゲッター
	const DirectionalLight& GetDirectionalLight() const { return *directionalLightData_; }
	
	// 自動更新制御
	void SetAutoUpdatePosition(bool enable) { autoUpdatePosition_ = enable; }
	void SetSceneCenter(const Vector3& center) { sceneCenter_ = center; }
	bool GetAutoUpdatePosition() const { return autoUpdatePosition_; }

	// PointLight
	void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);
  void SetPointLightColor(const Vector4& color, int index) { pointLightDatas_[index].color = color; }
  void SetPointLightPos(const Vector3& position, int index) { pointLightDatas_[index].position = position; }
  void SetPointLightIntensity(float intensity, int index) { pointLightDatas_[index].intensity = intensity; }
  void SetPointLightRadius(float radius, int index) { pointLightDatas_[index].radius = radius; }
  void SetPointLightDecay(float decay, int index) { pointLightDatas_[index].decay = decay; }
  void SetPointLightEnable(bool enable, int index) { pointLightDatas_[index].enable = enable; }

	// SpotLight
	void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index);
  void SetSpotLightColor(const Vector4& color, int index) { spotLightData_[index].color = color; }
  void SetSpotLightPos(const Vector3& position, int index) { spotLightData_[index].position = position; }
  void SetSpotLightIntensity(float intensity, int index) { spotLightData_[index].intensity = intensity; }
  void SetSpotLightDirection(const Vector3& direction, int index) { spotLightData_[index].direction = direction; }
  void SetSpotLightDistance(float distance, int index) { spotLightData_[index].distance = distance; }
  void SetSpotLightDecay(float decay, int index) { spotLightData_[index].decay = decay; }
  void SetSpotLightCosAngle(float cosAngle, int index) { spotLightData_[index].cosAngle = cosAngle; }
  void SetSpotLightEnable(bool enable, int index) { spotLightData_[index].enable = enable; }


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
  uint32_t pointLightSrvIndex_;

	// スポットライトのsrvIndex
  uint32_t spotLightSrvIndex_;

  // pointLightのindexのリスト
  std::vector<uint32_t> pointLightIndexList_;

  // spotLightのindexのリスト
  std::vector<uint32_t> spotLightIndexList_;
  
  // シャドウマップ自動更新関連
  bool autoUpdatePosition_ = true;  // 位置の自動更新フラグ
  Vector3 sceneCenter_ = {0.0f, 0.0f, 0.0f};  // シーン中心位置
};