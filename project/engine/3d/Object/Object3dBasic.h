#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>

#include "Camera.h"
#include "Matrix4x4.h"
#include "Light.h"
#include "SkyBox.h"

class DX12Basic;

namespace Tako {

class Camera;

/// <summary>
/// 3D オブジェクト描画の基盤システムクラス
/// シングルトンパターンで実装され、通常描画とインスタンシング描画の両方をサポート
/// カメラ、ライティング、デバッグ機能を統合管理
/// </summary>
class Object3dBasic {
private: // シングルトン設定

	// インスタンス
	static std::unique_ptr<Object3dBasic> instance_;

	Object3dBasic() = default;
	~Object3dBasic() = default;
	Object3dBasic(Object3dBasic&) = delete;
	Object3dBasic& operator=(Object3dBasic&) = delete;

	friend struct std::default_delete<Object3dBasic>;

public: // メンバー関数

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static Object3dBasic* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 共通描画設定
	/// </summary>
	void SetCommonRenderSetting();

	/// <summary>
	/// インスタンシング描画設定
	/// </summary>
	void SetInstancedRenderSetting();

	/// <summary>
	/// 半透明描画設定 (CullMode=NONE, DepthWriteMask=ZERO で両面描画 + 深度書き込み無効)
	/// </summary>
	void SetTransparentRenderSetting();


	// ===== Getters =====
	/// <summary>
	/// DirectX12基盤システムを取得
	/// </summary>
	/// <returns>DX12Basic ポインタ</returns>
	DX12Basic* GetDX12Basic() const { return m_dx12_; }

	/// <summary>
	/// カメラのポインタのポインタを取得
	/// </summary>
	/// <returns>カメラポインタのポインタ</returns>
	Camera** GetCamera() { return &camera_; }

	/// <summary>
	/// デバッグモードの状態を取得
	/// </summary>
	/// <returns>デバッグモードが有効な場合 true</returns>
	bool GetDebug() const { return isDebug_; }

	/// <summary>
	/// ライトシステムを取得
	/// </summary>
	/// <returns>Light ポインタ</returns>
	Light* GetLight() const { return light_.get(); }

	// ===== Setters =====
	/// <summary>
	/// カメラを設定
	/// </summary>
	/// <param name="camera">カメラポインタ</param>
	void SetCamera(Camera* camera) { camera_ = camera; }

	/// <summary>
	/// カメラの座標を設定
	/// </summary>
	/// <param name="translate">座標値</param>
	void SetCameraTranslate(const Vector3& translate) { camera_->SetTranslate(translate); }

	/// <summary>
	/// カメラの回転を設定
	/// </summary>
	/// <param name="rotation">回転値（ラジアン）</param>
	void SetCameraRotation(const Vector3& rotation) { camera_->SetRotate(rotation); }

	/// <summary>
	/// デバッグモードを設定
	/// </summary>
	/// <param name="isDebug">デバッグモードを有効にするか</param>
	void SetDebug(bool isDebug) { isDebug_ = isDebug; }

	// ===== DirectionalLight =====
	/// <summary>
	/// 平行光源を設定
	/// </summary>
	/// <param name="direction">ライト方向</param>
	/// <param name="color">ライトカラー</param>
	/// <param name="lightType">ライトタイプ</param>
	/// <param name="intensity">ライト強度</param>
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);

	/// <summary>
	/// 平行光源の方向を設定
	/// </summary>
	/// <param name="direction">ライト方向</param>
	void SetDirectionalLightDirection(const Vector3& direction) { light_->SetDirectionalLightDirection(direction); }

	/// <summary>
	/// 平行光源のカラーを設定
	/// </summary>
	/// <param name="color">ライトカラー</param>
	void SetDirectionalLightColor(const Vector4& color) { light_->SetDirectionalLightColor(color); }

	/// <summary>
	/// 平行光源のタイプを設定
	/// </summary>
	/// <param name="lightType">ライトタイプ</param>
	void SetDirectionalLightType(int32_t lightType) { light_->SetDirectionalLightType(lightType); }

	/// <summary>
	/// 平行光源の強度を設定
	/// </summary>
	/// <param name="intensity">ライト強度</param>
	void SetDirectionalLightIntensity(float intensity) { light_->SetDirectionalLightIntensity(intensity); }

	// ===== PointLight =====
	/// <summary>
	/// 点光源を設定
	/// </summary>
	/// <param name="position">位置</param>
	/// <param name="color">カラー</param>
	/// <param name="intensity">強度</param>
	/// <param name="radius">半径</param>
	/// <param name="decay">減衰率</param>
	/// <param name="enable">有効フラグ</param>
	/// <param name="index">インデックス</param>
	void SetPointLight(const Vector3& position, const Vector4& color, float intensity, float radius, float decay, bool enable, int index);

	/// <summary>
	/// 点光源のカラーを設定
	/// </summary>
	/// <param name="color">カラー</param>
	/// <param name="index">インデックス</param>
	void SetPointLightColor(const Vector4& color, int index) { light_->SetPointLightColor(color, index); }

	/// <summary>
	/// 点光源の位置を設定
	/// </summary>
	/// <param name="position">位置</param>
	/// <param name="index">インデックス</param>
	void SetPointLightPos(const Vector3& position, int index) { light_->SetPointLightPos(position, index); }

	/// <summary>
	/// 点光源の強度を設定
	/// </summary>
	/// <param name="intensity">強度</param>
	/// <param name="index">インデックス</param>
	void SetPointLightIntensity(float intensity, int index) { light_->SetPointLightIntensity(intensity, index); }

	/// <summary>
	/// 点光源の半径を設定
	/// </summary>
	/// <param name="radius">半径</param>
	/// <param name="index">インデックス</param>
	void SetPointLightRadius(float radius, int index) { light_->SetPointLightRadius(radius, index); }

	/// <summary>
	/// 点光源の減衰率を設定
	/// </summary>
	/// <param name="decay">減衰率</param>
	/// <param name="index">インデックス</param>
	void SetPointLightDecay(float decay, int index) { light_->SetPointLightDecay(decay, index); }

	/// <summary>
	/// 点光源の有効化を設定
	/// </summary>
	/// <param name="enable">有効フラグ</param>
	/// <param name="index">インデックス</param>
	void SetPointLightEnable(bool enable, int index) { light_->SetPointLightEnable(enable, index); }

	// ===== SpotLight =====
	/// <summary>
	/// スポットライトを設定
	/// </summary>
	/// <param name="position">位置</param>
	/// <param name="direction">方向</param>
	/// <param name="color">カラー</param>
	/// <param name="intensity">強度</param>
	/// <param name="distance">距離</param>
	/// <param name="decay">減衰率</param>
	/// <param name="cosAngle">コーン角のコサイン値</param>
	/// <param name="enable">有効フラグ</param>
	/// <param name="index">インデックス</param>
	void SetSpotLight(const Vector3& position, const Vector3& direction, const Vector4& color, float intensity, float distance, float decay, float cosAngle, bool enable, int index);

	/// <summary>
	/// スポットライトのカラーを設定
	/// </summary>
	/// <param name="color">カラー</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightColor(const Vector4& color, int index) { light_->SetSpotLightColor(color, index); }

	/// <summary>
	/// スポットライトの位置を設定
	/// </summary>
	/// <param name="position">位置</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightPos(const Vector3& position, int index) { light_->SetSpotLightPos(position, index); }

	/// <summary>
	/// スポットライトの強度を設定
	/// </summary>
	/// <param name="intensity">強度</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightIntensity(float intensity, int index) { light_->SetSpotLightIntensity(intensity, index); }

	/// <summary>
	/// スポットライトの距離を設定
	/// </summary>
	/// <param name="distance">距離</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightDistance(float distance, int index) { light_->SetSpotLightDistance(distance, index); }

	/// <summary>
	/// スポットライトの減衰率を設定
	/// </summary>
	/// <param name="decay">減衰率</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightDecay(float decay, int index) { light_->SetSpotLightDecay(decay, index); }

	/// <summary>
	/// スポットライトのコーン角を設定
	/// </summary>
	/// <param name="cosAngle">コサイン値</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightCosAngle(float cosAngle, int index) { light_->SetSpotLightCosAngle(cosAngle, index); }

	/// <summary>
	/// スポットライトの有効化を設定
	/// </summary>
	/// <param name="enable">有効フラグ</param>
	/// <param name="index">インデックス</param>
	void SetSpotLightEnable(bool enable, int index) { light_->SetSpotLightEnable(enable, index); }

	// ===== Shadow Mapping =====
	/// <summary>
	/// 平行光源の位置を設定
	/// </summary>
	/// <param name="position">ライト位置</param>
	void SetDirectionalLightPosition(const Vector3& position) { light_->SetDirectionalLightPosition(position); }

	/// <summary>
	/// 自動更新位置を設定
	/// </summary>
	/// <param name="enable">有効フラグ</param>
	void SetAutoUpdatePosition(bool enable) { light_->SetAutoUpdatePosition(enable); }

	/// <summary>
	/// シーン中心を設定
	/// </summary>
	/// <param name="center">中心座標</param>
	void SetSceneCenter(const Vector3& center) { light_->SetSceneCenter(center); }

private: // プライベートメンバー関数

	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// パイプラインステートの生成
	/// </summary>
	void CreatePSO();

	/// <summary>
	/// インスタンシング用ルートシグネチャの作成
	/// </summary>
	void CreateInstancedRootSignature();

	/// <summary>
	/// インスタンシング用パイプラインステートの生成
	/// </summary>
	void CreateInstancedPSO();

	/// <summary>
	/// 半透明描画用パイプラインステートの生成 (CullMode=NONE, DepthWriteMask=ZERO)
	/// RootSignature は通常描画用と共有
	/// </summary>
	void CreateTransparentPSO();

private: // メンバー変数
	DX12Basic* m_dx12_ = nullptr; ///< DirectX12基盤システムへの参照

	Camera* camera_ = nullptr; ///< デフォルトカメラへのポインタ

	std::unique_ptr<Light> light_; ///< ライティングシステムへのポインタ

	Matrix4x4 viewProjectionMatrix_; ///< ビュープロジェクション行列
	Matrix4x4 debugViewProjectionMatrix_; ///< デバッグ表示用ビュープロジェクション行列

	bool isDebug_ = false; ///< デバッグモード有効フラグ

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; ///< 通常描画用ルートシグネチャ

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; ///< 通常描画用パイプラインステート

	Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_; ///< インスタンシング描画用ルートシグネチャ

	Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_; ///< インスタンシング描画用パイプラインステート

	Microsoft::WRL::ComPtr<ID3D12PipelineState> transparentPipelineState_; ///< 半透明描画用パイプラインステート (両面描画 + 深度書き込み無効)
};

} // namespace Tako
