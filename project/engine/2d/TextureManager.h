#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<wrl.h>
#include <d3d12.h>
#include"externals/DirectXTex/DirectXTex.h"

class DX12Basic;

class SrvManager;

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
		std::string fileName;
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		uint32_t srvIndex;
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
	void Initialize(DX12Basic* dx12, std::string directoryPath);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	void LoadTexture(const std::string& fileName);

	/// <summary>
	/// テクスチャのインデックスからGPUハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(const std::string& fileName);

	/// <summary>
	/// メタデータを取得
	/// </summary>
	const DirectX::TexMetadata& GetMetaData(const std::string& fileName);

	/// <summary>
	/// srvIndexを取得
	/// </summary>
	uint32_t GetSRVIndex(const std::string& fileName);

private: // メンバー変数

	// DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	// テクスチャ格納ディレクトリ
	std::string directoryPath_;

	// テクスチャデータ配列
	std::unordered_map<std::string, TextureData> textureDatas_;

};
