#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "StringUtility.h"
#include "EnginePaths.h"
#include <algorithm>
#include <cassert>

namespace Tako {

  std::unique_ptr<TextureManager> TextureManager::instance_ = nullptr;

  TextureManager* TextureManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<TextureManager>(Token{});
    }
    return instance_.get();
  }

  void TextureManager::Initialize(DX12Basic* dx12, const std::string& directoryPath)
  {
    dx12_ = dx12;

    directoryPath_ = directoryPath;

    textureData_.reserve(SrvManager::kMaxSRVCount);
  }

  void TextureManager::Finalize()
  {
    // 全テクスチャの SRV を返却
    for (auto& [name, data] : textureData_) {
      SrvManager::GetInstance()->Free(data.srvIndex);
    }

    instance_.reset();
  }

  void TextureManager::LoadTexture(const std::string& fileName)
  {
    // 重複チェック
    if (textureData_.contains(fileName)) {
      return;
    }

    // テクスチャ枚数上限チェック
    assert(SrvManager::GetInstance()->CanAllocate());

    HRESULT hr;

    // テクスチャの読み込み
    // fileName が "EngineResources/" で始まる場合は directoryPath_ を経由せず、エンジン用パスとして直接読む
    DirectX::ScratchImage image;
    const std::string filePath = fileName.starts_with("EngineResources/") ? fileName : (directoryPath_ + fileName);
    std::wstring filePathW = StringUtility::ConvertString(filePath);

    if (filePathW.ends_with(L".dds")) {
      // DDS ファイルの読み込み
      hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
      assert(SUCCEEDED(hr));
    }
    else {
      // WIC ファイルの読み込み
      hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
      assert(SUCCEEDED(hr));
    }

    // mipmap を生成
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
      mipImages = std::move(image);
    }
    else {
      // 非圧縮テクスチャの場合
      hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 4, mipImages);
    }
    assert(SUCCEEDED(hr));

    // 追加したテクスチャデータを取得
    TextureData& textureData = textureData_[fileName];

    textureData.fileName = fileName;
    textureData.metadata = mipImages.GetMetadata();
    textureData.resource = dx12_->MakeTextureResource(textureData.metadata);
    textureData.intermediateResource = dx12_->UploadTextureData(textureData.resource, mipImages);

    // テクスチャデータの SRV インデックスを設定
    textureData.srvIndex = SrvManager::GetInstance()->Allocate();

    // テクスチャデータの SRV ハンドルを取得
    textureData.srvCpuHandle = SrvManager::GetInstance()->GetCPUDescriptorHandle(textureData.srvIndex);
    textureData.srvGpuHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(textureData.srvIndex);

    // SRV の作成
    if (textureData.metadata.IsCubemap()) {
      // キューブマップの場合
      SrvManager::GetInstance()->CreateSRVForCubeMap(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, UINT_MAX);
    }
    else {
      // 通常の2D テクスチャの場合
      SrvManager::GetInstance()->CreateSRVForTexture2D(textureData.srvIndex, textureData.resource.Get(), textureData.metadata.format, static_cast<UINT>(textureData.metadata.mipLevels));
    }
  }

  D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSRVGPUHandle(const std::string& fileName)
  {
    TextureData& textureData = textureData_[fileName];

    return textureData.srvGpuHandle;
  }

  const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& fileName)
  {
    TextureData& textureData = textureData_[fileName];

    return textureData.metadata;
  }

  const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t srvIndex)
  {
    // SRV インデックスからテクスチャデータを検索
    auto it = std::find_if(
      textureData_.begin(), textureData_.end(),
      [srvIndex](const auto& pair) { return pair.second.srvIndex == srvIndex; });

    assert(it != textureData_.end() && "TextureManager::GetMetaData: srvIndex not found");

    return it->second.metadata;
  }

  uint32_t TextureManager::GetSRVIndex(const std::string& fileName)
  {
    TextureData& textureData = textureData_[fileName];

    return textureData.srvIndex;
  }

  const std::string& TextureManager::GetFileName(uint32_t srvIndex)
  {
    // SRV インデックスからテクスチャデータを検索
    auto it = std::find_if(
      textureData_.begin(), textureData_.end(),
      [srvIndex](const auto& pair) { return pair.second.srvIndex == srvIndex; });

    if (it == textureData_.end()) {
      static const std::string empty;
      return empty;
    }

    return it->second.fileName;
  }

  std::vector<std::string> TextureManager::GetLoadedTextureFileNames() const
  {
    std::vector<std::string> names;
    names.reserve(textureData_.size());
    for (const auto& [name, data] : textureData_) {
      (void)data;
      names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
  }

  void TextureManager::LoadEngineDefault(const std::string& fileName)
  {
    // エンジン用フルパスをキー兼ファイルパスに使う（LoadTexture 内のプレフィックス判定で directoryPath_ を経由しない）
    LoadTexture(EnginePaths::TexturePath(fileName));
  }

  D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetEngineDefaultSRVGPUHandle(const std::string& fileName)
  {
    return GetSRVGPUHandle(EnginePaths::TexturePath(fileName));
  }

  const DirectX::TexMetadata& TextureManager::GetEngineDefaultMetaData(const std::string& fileName)
  {
    return GetMetaData(EnginePaths::TexturePath(fileName));
  }

  uint32_t TextureManager::GetEngineDefaultSRVIndex(const std::string& fileName)
  {
    return GetSRVIndex(EnginePaths::TexturePath(fileName));
  }

} // namespace Tako
