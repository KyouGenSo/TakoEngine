#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<wrl.h>
#include <d3d12.h>
#include"DirectXTex.h"

class DX12Basic;

class SrvManager;

/// <summary>
/// テクスチャの読み込みと管理を行うシングルトンクラス。テクスチャのキャッシュとSRV管理を担当
/// </summary>
class TextureManager{
private: // シングルトン設定

	///< インスタンス
	static TextureManager* instance_;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

private: // 構造体

	/// <summary>
	/// テクスチャデータ構造体
	/// </summary>
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

	///< SRVIndexの開始番号
	static uint32_t kSRVIndexStart;

public: // メンバー関数

	/// <summary>
	/// インスタンスの取得
	/// </summary>
	static TextureManager* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="dx12">DX12Basicのインスタンス</param>
	/// <param name="directoryPath">テクスチャ格納ディレクトリのパス</param>
	void Initialize(DX12Basic* dx12, const std::string& directoryPath);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// テクスチャファイルの読み込み
	/// </summary>
	/// <param name="fileName">読み込むテクスチャファイルの名前</param>
	void LoadTexture(const std::string& fileName);

	/// <summary>
	/// テクスチャのインデックスからGPUハンドルを取得
	/// </summary>
	/// <param name="fileName">テクスチャファイルの名前</param>
	/// <returns>SRVのGPUディスクリプタハンドル</returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(const std::string& fileName);

	/// <summary>
	/// メタデータを取得
	/// </summary>
	/// <param name="fileName">テクスチャファイルの名前</param>
	/// <returns>テクスチャのメタデータ</returns>
	const DirectX::TexMetadata& GetMetaData(const std::string& fileName);

	/// <summary>
	/// srvIndexを取得
	/// </summary>
	/// <param name="fileName">テクスチャファイルの名前</param>
	/// <returns>SRVインデックス</returns>
	uint32_t GetSRVIndex(const std::string& fileName);

private: // メンバー変数

	///< DX12Basicクラスのインスタンス
	DX12Basic* m_dx12_ = nullptr;

	///< テクスチャ格納ディレクトリ
	std::string directoryPath_;

	///< テクスチャデータ配列
	std::unordered_map<std::string, TextureData> textureDatas_;

};
