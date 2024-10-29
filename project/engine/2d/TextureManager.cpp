#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "StringUtility.h"
#include <algorithm>
#include <cassert>

TextureManager* TextureManager::instance_ = nullptr;

uint32_t TextureManager::kSRVIndexStart = 1;

TextureManager* TextureManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new TextureManager();
	}
	return instance_;
}

void TextureManager::Initialize(DX12Basic* dx12, std::string directoryPath)
{
	m_dx12_ = dx12;

	directoryPath_ = directoryPath;

	textureDatas_.reserve(DX12Basic::kMaxSRVCount);
}

void TextureManager::Finalize()
{
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void TextureManager::LoadTexture(const std::string& fileName)
{
	// 読み込み済みのテクスチャを検索
	if (textureDatas_.contains(fileName))
	{
		// すでに読み込まれている場合は何もしない
		return;
	}

	// テクスチャ枚数上限チェック
	assert(SrvManager::GetInstance()->CanAllocate());
	
	// テクスチャの読み込み
	DirectX::ScratchImage image;
	std::wstring filePathW = StringUtility::ConvertString(directoryPath_ + fileName);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// mipmapを生成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// 追加したテクスチャデータを取得
	TextureData& textureData = textureDatas_[fileName];

	textureData.fileName = fileName;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = m_dx12_->MakeTextureResource(textureData.metadata);
	textureData.intermediateResource = m_dx12_->UploadTextureData(textureData.resource, mipImages);

	// テクスチャデータのSRVインデックスを設定
	textureData.srvIndex = SrvManager::GetInstance()->Allocate();

	// テクスチャデータのSRVハンドルを取得
	textureData.srvCpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvGpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(textureData.srvIndex);

	// SRVの作成
	SrvManager::GetInstance()->CreateSRVForTexture2D(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, UINT(textureData.metadata.mipLevels));
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSRVGPUHandle(const std::string& fileName)
{
	// クスチャデータを取得
	TextureData& textureData = textureDatas_[fileName];

	return textureData.srvGpuHandle;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& fileName)
{
	// クスチャデータを取得
	TextureData& textureData = textureDatas_[fileName];

	return textureData.metadata;
}

uint32_t TextureManager::GetSRVIndex(const std::string& fileName)
{
	// クスチャデータを取得
	TextureData& textureData = textureDatas_[fileName];

	return textureData.srvIndex;
}
