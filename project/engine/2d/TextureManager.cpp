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

void TextureManager::Initialize(DX12Basic* dx12, SrvManager* srvManager)
{
	m_dx12_ = dx12;

	m_srvManager_ = srvManager;

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

void TextureManager::LoadTexture(const std::string& filePath)
{
	// 読み込み済みのテクスチャを検索
	if (textureDatas_.contains(filePath))
	{
		// すでに読み込まれている場合は何もしない
		return;
	}

	// テクスチャ枚数上限チェック
	assert(m_srvManager_->CanAllocate());
	
	// テクスチャの読み込み
	DirectX::ScratchImage image;
	std::wstring filePathW = StringUtility::ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// mipmapを生成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// 追加したテクスチャデータを取得
	TextureData& textureData = textureDatas_[filePath];

	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = m_dx12_->MakeTextureResource(textureData.metadata);
	textureData.intermediateResource = m_dx12_->UploadTextureData(textureData.resource, mipImages);

	// テクスチャデータのSRVインデックスを設定
	textureData.srvIndex = m_srvManager_->Allocate();

	// テクスチャデータのSRVハンドルを取得
	textureData.srvCpuHandle = m_srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvGpuHandle = m_srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	// SRVの作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);

	m_dx12_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvCpuHandle);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSRVGPUHandle(const std::string& filePath)
{
	// クスチャデータを取得
	TextureData& textureData = textureDatas_[filePath];

	return textureData.srvGpuHandle;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	// クスチャデータを取得
	TextureData& textureData = textureDatas_[filePath];

	return textureData.metadata;
}

uint32_t TextureManager::GetSRVIndex(const std::string& filePath)
{
	// クスチャデータを取得
	TextureData& textureData = textureDatas_[filePath];

	return textureData.srvIndex;
}
