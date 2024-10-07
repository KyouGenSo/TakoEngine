#pragma once
#include<string>
#include<vector>
#include<wrl.h>
#include <d3d12.h>
#include"externals/DirectXTex/DirectXTex.h"

class DX12Basic;

class TextureManager{
private: // シングルトン設定

	// インスタンス
	static TextureManager* instance_;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

private: // 構造体

	struct TextureData
	{
		std::string filePath;
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle;
	};

public: // 静的メンバー変数

	// SRVIndexの開始番号
	static uint32_t kSRVIndexStart;

public: // メンバー関数
	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static TextureManager* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DX12Basic* dx12);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	void LoadTexture(const std::string& filePath);

	/// <summary>
	/// テクスチャのインデックスを取得
	/// </summary>
	uint32_t GetTextureIndex(const std::string& filePath);

	/// <summary>
	/// テクスチャのインデックスからGPUハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpuHandle(uint32_t textureIndex);

private: // メンバー変数

	// DX12Basicクラスのインスタンス
	DX12Basic* dx12_;

	// テクスチャデータ配列
	std::vector<TextureData> textureDatas_;

};
