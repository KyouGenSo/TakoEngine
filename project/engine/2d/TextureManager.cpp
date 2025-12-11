#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "StringUtility.h"
#include <algorithm>
#include <cassert>

namespace Tako {

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

void TextureManager::Initialize(DX12Basic* dx12, const std::string& directoryPath)
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
	// 重複チェック
	if (textureDatas_.contains(fileName))
	{
		return;
	}

	// テクスチャ枚数上限チェック
	assert(SrvManager::GetInstance()->CanAllocate());

  HRESULT hr;

	// テクスチャの読み込み
	DirectX::ScratchImage image;
	std::wstring filePathW = StringUtility::ConvertString(directoryPath_ + fileName);

  if (filePathW.ends_with(L".dds"))
  {
    // DDSファイルの読み込み
    hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    assert(SUCCEEDED(hr));
  }
  else
  {
    // WICファイルの読み込み
    hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));
  }

	// mipmapを生成
	DirectX::ScratchImage mipImages{};
  if (DirectX::IsCompressed(image.GetMetadata().format))
  {
    mipImages = std::move(image);
  }
  else
  {
    // 非圧縮テクスチャの場合
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 4, mipImages);
  }
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
  if (textureData.metadata.IsCubemap())
  {
    // キューブマップの場合
    SrvManager::GetInstance()->CreateSRVForCubeMap(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, UINT_MAX);
  } else
  {
    // 通常の2Dテクスチャの場合
    SrvManager::GetInstance()->CreateSRVForTexture2D(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
  }
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

} // namespace Tako
