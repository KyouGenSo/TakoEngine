#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>

#include "Camera.h"
#include "Matrix4x4.h"
#include "Light.h"
#include "SkyBox.h"
#include "PerFrameConstantRing.h"

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

	struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
	~Object3dBasic() = default;
	Object3dBasic(Object3dBasic&) = delete;
	Object3dBasic& operator=(Object3dBasic&) = delete;

	friend struct std::default_delete<Object3dBasic>;

public:
	explicit Object3dBasic(Token) {}

public: //定数

	// 通常/インスタンシング共通のルートパラメータ番号
	static constexpr UINT kMaterialParam         = 0;   ///< b0 (PS): マテリアル
	static constexpr UINT kTransformParam        = 1;   ///< b0 (VS): 変換行列（通常 RS）
	static constexpr UINT kTextureParam          = 2;   ///< t0 (PS): テクスチャ
	static constexpr UINT kDirectionalLightParam = 3;   ///< b1 (PS): 平行光源
	static constexpr UINT kViewParam             = 4;   ///< b2 (ALL): 視点（VP・カメラ位置）
	static constexpr UINT kPointLightParam       = 5;   ///< t1 (PS): 点光源
	static constexpr UINT kSpotLightParam        = 6;   ///< t2 (PS): スポットライト
	static constexpr UINT kLightConstantsParam   = 7;   ///< b3 (PS): ライト数
	static constexpr UINT kEnvMapParam           = 8;   ///< t3 (PS): 環境マップ
	static constexpr UINT kShadowConstantsParam  = 9;   ///< b4 (ALL): シャドウ定数
	static constexpr UINT kShadowMapParam        = 10;  ///< t4 (PS): シャドウマップ

	// インスタンシング RS 専用（通常 RS の変換行列の枠を使う）
	static constexpr UINT kInstanceDataParam = 1;  ///< t5 (VS): インスタンスデータ

private: //定数
	static constexpr uint32_t kMaxViewsPerFrame = 4;  ///< 1 フレームで SetView できる回数（本編/デバッグビュー/プレビュー）

public: //構造体
	/// <summary>
	/// 視点ごとの定数（View.hlsli と一致させること）
	/// </summary>
	struct ViewConstants {
		Matrix4x4 viewProjection;
		Vector3   worldPosition;
		float     padding;
	};

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

	//============================================================
	//Setter
	//============================================================
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

	/// <summary>
	/// 以降の描画設定でバインドする視点を切り替える。Update で本編カメラに戻る
	/// </summary>
	/// <param name="camera">描画に使うカメラ（view × projection を合成して使う）</param>
	void SetView(const Camera& camera);

	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetCameraTranslate(const Vector3& translate) { camera_->SetTranslate(translate); }
	void SetCameraRotation(const Vector3& rotation) { camera_->SetRotate(rotation); }

	// DirectionalLight
	/// <summary>
	/// 平行光源を設定
	/// </summary>
	/// <param name="direction">ライト方向</param>
	/// <param name="color">ライトカラー</param>
	/// <param name="lightType">ライトタイプ</param>
	/// <param name="intensity">ライト強度</param>
	void SetDirectionalLight(const Vector3& direction, const Vector4& color, int32_t lightType, float intensity);

	void SetDirectionalLightDirection(const Vector3& direction) { light_->SetDirectionalLightDirection(direction); }
	void SetDirectionalLightColor(const Vector4& color) { light_->SetDirectionalLightColor(color); }
	void SetDirectionalLightType(int32_t lightType) { light_->SetDirectionalLightType(lightType); }
	void SetDirectionalLightIntensity(float intensity) { light_->SetDirectionalLightIntensity(intensity); }

	// PointLight
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

	void SetPointLightColor(const Vector4& color, int index) { light_->SetPointLightColor(color, index); }
	void SetPointLightPos(const Vector3& position, int index) { light_->SetPointLightPos(position, index); }
	void SetPointLightIntensity(float intensity, int index) { light_->SetPointLightIntensity(intensity, index); }
	void SetPointLightRadius(float radius, int index) { light_->SetPointLightRadius(radius, index); }
	void SetPointLightDecay(float decay, int index) { light_->SetPointLightDecay(decay, index); }
	void SetPointLightEnable(bool enable, int index) { light_->SetPointLightEnable(enable, index); }

	// SpotLight
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

	void SetSpotLightColor(const Vector4& color, int index) { light_->SetSpotLightColor(color, index); }
	void SetSpotLightPos(const Vector3& position, int index) { light_->SetSpotLightPos(position, index); }
	void SetSpotLightIntensity(float intensity, int index) { light_->SetSpotLightIntensity(intensity, index); }
	void SetSpotLightDistance(float distance, int index) { light_->SetSpotLightDistance(distance, index); }
	void SetSpotLightDecay(float decay, int index) { light_->SetSpotLightDecay(decay, index); }
	void SetSpotLightCosAngle(float cosAngle, int index) { light_->SetSpotLightCosAngle(cosAngle, index); }
	void SetSpotLightEnable(bool enable, int index) { light_->SetSpotLightEnable(enable, index); }

	// Shadow Mapping
	void SetDirectionalLightPosition(const Vector3& position) { light_->SetDirectionalLightPosition(position); }
	void SetAutoUpdatePosition(bool enable) { light_->SetAutoUpdatePosition(enable); }
	void SetSceneCenter(const Vector3& center) { light_->SetSceneCenter(center); }

	//============================================================
	//Getter
	//============================================================
	DX12Basic* GetDX12Basic() const { return dx12_; }
	Camera** GetCamera() { return &camera_; }
	Light* GetLight() const { return light_.get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetViewAddress() const { return currentViewAddress_; }

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
	DX12Basic*                                  dx12_                    = nullptr;  ///< DirectX12基盤システムへの参照
	Camera*                                     camera_                    = nullptr;  ///< デフォルトカメラへのポインタ
	std::unique_ptr<Light>                      light_;                                ///< ライティングシステムへのポインタ
	PerFrameConstantRing<ViewConstants>         viewRing_;                             ///< 視点 CB（1 フレームで複数視点を併存させる）
	D3D12_GPU_VIRTUAL_ADDRESS                   currentViewAddress_        = 0;        ///< 描画設定でバインドする視点 CB
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;                        ///< 通常描画用ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;                        ///< 通常描画用パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_;               ///< インスタンシング描画用ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_;               ///< インスタンシング描画用パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> transparentPipelineState_;             ///< 半透明描画用パイプラインステート (両面描画 + 深度書き込み無効)
};

} // namespace Tako
