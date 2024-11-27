#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include<unordered_map>
#include<string>
#include<wrl.h>
#include"Vector2.h"
#include"Vector3.h"
#include"Vector4.h"
#include "numbers"

class DX12Basic;

class PostEffect {
private:
	// シングルトン設定
	static PostEffect* instance_;

	PostEffect() = default;
	~PostEffect() = default;
	PostEffect(PostEffect&) = delete;
	PostEffect& operator=(PostEffect&) = delete;

public: // メンバ関数

	struct VignetteParam
	{
		float power;
		float range;
	};

	struct VignetteRedBloomParam
	{
		float power;
		float range;
		float threshold;
	};

	struct BloomParam
	{
		float intensity;
		float threshold;
		float sigma;
	};

	struct PixelateParam
	{
		float pixelSize;
	};

	// Shader用のカメラ
	struct CameraForGPU
	{
		float nearPlane;
		float farPlane;
	};

	struct FogParam
	{
		Vector4 color;
		float density;
	};

	// ComPtrのエイリアス
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// インスタンスの取得
	static PostEffect* GetInstance();

	// 初期化
	void Initialize(DX12Basic* dx12);

	// 終了処理
	void Finalize();

	// 描画前の処理
	void BeginDraw();

	// 描画
	void Draw(const std::string& effectName);
	//void DrawWithMultipleEffects(const std::vector<std::string>& effectNames);

	// バリアの設定
	void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
	void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, ID3D12Resource* resource);

	// レンダーテクスチャの取得
	ID3D12Resource* GetRenderTextureResource() { return renderTextureResourceA_.Get(); }

	void SetVignettePower(float power);

	void SetVignetteRange(float range);

	void SetBloomThreshold(float threshold);

	void SetBloomIntensity(float intensity);

	void SetBloomSigma(float sigma);

	void SetFogColor(const Vector4& color);

	void SetFogDensity(float density);

private: // プライベートメンバー関数

	// レンダーテクスチャの初期化
	void InitRenderTexture();

	// 深度バッファのSRVを生成
	void CreateDepthBufferSRV();

	// ルートシグネチャの生成
	void CreateRootSignature(const std::string& effectName);

	// パイプラインステートの生成
	void CreatePSO(const std::string& effectName);

	// VignetteParamを生成
	void CreateVignetteParam();

	// VignetteRedBloomParamを生成
	void CreateVignetteRedBloomParam();

	// BloomParamを生成
	void CreateBloomParam();

	// FogParamを生成
	void CreateFogParam();

	// CameraForGPUを生成
	void CreateCameraForGPU();

	// パラメーターリソースの設定
	void SetParamResource(const std::string& effectName);

private: // メンバ変数

	// DX12の基本情報
	DX12Basic* m_dx12_ = nullptr;

	// レンダーテクスチャリソース
	ComPtr<ID3D12Resource> renderTextureResourceA_;
	ComPtr<ID3D12Resource> renderTextureResourceB_;

	// レンダーテクスチャの RTV ハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRTVHandleA_;
	D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRTVHandleB_;

	// レンダーテクスチャのclearColor
	const Vector4 kRenderTextureClearColor_ = { 0.05f, 0.05f, 0.05f, 1.0f };

	// ルートシグネチャ
	std::unordered_map < std::string, ComPtr<ID3D12RootSignature>> rootSignatures_;

	// パイプラインステート
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelineStates_;

	// シェーダーリソースビューのインデックス
	uint32_t rtvSrvIndex_ = 0;
	uint32_t dsvSrvIndex_ = 0;

	// パラメーターリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vignetteParamResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vignetteRedBloomParamResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> bloomParamResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> fogParamResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraForGPUResource_;

	// パラメーターデータ
	VignetteParam* vignetteParam_;
	VignetteRedBloomParam* vignetteRedBloomParam_;
	BloomParam* bloomParam_;
	FogParam* fogParam_;
	CameraForGPU* cameraForGPU_;

};
