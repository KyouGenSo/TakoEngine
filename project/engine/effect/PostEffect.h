#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include<unordered_map>
#include<string>
#include<wrl.h>
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

	// バリアの設定
	void SetBarrier(D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	// レンダーテクスチャの取得
	ID3D12Resource* GetRenderTextureResource() { return renderTextureResource_.Get(); }

	void SetVignettePower(float power);

private: // プライベートメンバー関数

	// レンダーテクスチャの初期化
	void InitRenderTexture();

	// ルートシグネチャの生成
	void CreateRootSignature(const std::string& effectName);

	// パイプラインステートの生成
	void CreatePSO(const std::string& effectName);

	// VignetteParamを生成
	void CreateVignetteParam();

private: // メンバ変数

	// DX12の基本情報
	DX12Basic* m_dx12_ = nullptr;

	// レンダーテクスチャリソース
	ComPtr<ID3D12Resource> renderTextureResource_;

	// レンダーテクスチャの RTV ハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRTVHandle_;

	// レンダーテクスチャのclearColor
	const Vector4 kRenderTextureClearColor_ = { 0.1f, 0.25f, 0.5f, 1.0f };

	// ルートシグネチャ
	std::unordered_map < std::string, ComPtr<ID3D12RootSignature>> rootSignatures_;

	// パイプラインステート
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelineStates_;

	// シェーダーリソースビューのインデックス
	uint32_t srvIndex_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> vignetteParamResource_;

	VignetteParam* vignetteParam_;

};
