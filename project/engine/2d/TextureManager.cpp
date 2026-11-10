#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "StringUtility.h"
#include "EnginePaths.h"
#include "Logger.h"
#include "externals/DirectXTex/d3dx12.h"
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
    DirectX::ScratchImage image;
    std::wstring filePathW = StringUtility::ConvertString(GetFilePath(fileName));

    if (filePathW.ends_with(L".dds")) {
      hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    }
    else {
      hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    if (FAILED(hr)) {
      // 毎フレーム引かれても記録は 1 回
      if (failedFiles_.insert(fileName).second) {
        Logger::Log("Texture not found or unreadable: %s", fileName.c_str());
      }
      return;
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
    textureData.resource = MakeTextureResource(textureData.metadata);
    textureData.intermediateResource = UploadTextureData(textureData.resource, mipImages);

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

  void TextureManager::Unload(const std::string& fileName)
  {
    auto it = textureData_.find(fileName);
    assert(it != textureData_.end() && "TextureManager::Unload: texture not loaded");
    if (it == textureData_.end()) {
      return;
    }

    SrvManager::GetInstance()->Free(it->second.srvIndex);
    textureData_.erase(it);
  }

  std::string TextureManager::GetFilePath(const std::string& fileName) const
  {
    // エンジン用テクスチャは directoryPath_ を経由せず直接読む
    return fileName.starts_with("EngineResources/") ? fileName : (directoryPath_ + fileName);
  }

  void TextureManager::ReleaseIntermediateResources()
  {
    for (auto& [name, data] : textureData_) {
      data.intermediateResource.Reset();
    }
  }

  const TextureManager::TextureData* TextureManager::Acquire(const std::string& fileName)
  {
    if (auto it = textureData_.find(fileName); it != textureData_.end()) {
      return &it->second;
    }
    if (!failedFiles_.contains(fileName)) {
      LoadTexture(fileName);
      if (auto it = textureData_.find(fileName); it != textureData_.end()) {
        return &it->second;
      }
    }
    auto fallback = textureData_.find(EnginePaths::TexturePath("white.png"));
    return fallback != textureData_.end() ? &fallback->second : nullptr;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSRVGPUHandle(const std::string& fileName)
  {
    const TextureData* data = Acquire(fileName);
    return data ? data->srvGpuHandle : D3D12_GPU_DESCRIPTOR_HANDLE{};
  }

  const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& fileName)
  {
    static const DirectX::TexMetadata empty{};
    const TextureData* data = Acquire(fileName);
    return data ? data->metadata : empty;
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
    const TextureData* data = Acquire(fileName);
    return data ? data->srvIndex : 0;
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

  GpuResource TextureManager::MakeTextureResource(const DirectX::TexMetadata& metaData)
  {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = static_cast<UINT>(metaData.width);
    resourceDesc.Height = static_cast<UINT>(metaData.height);
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(metaData.arraySize);
    resourceDesc.MipLevels = static_cast<UINT16>(metaData.mipLevels);
    resourceDesc.Format = metaData.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metaData.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    GpuResource texture;
    HRESULT hr = dx12_->GetDevice()->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&texture.resource));
    assert(SUCCEEDED(hr));
    texture.state = D3D12_RESOURCE_STATE_COPY_DEST;
    return texture;
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(GpuResource& texture, const DirectX::ScratchImage& mipImages)
  {
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(dx12_->GetDevice(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

    uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast<UINT>(subresources.size()));
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dx12_->MakeBufferResource(intermediateSize);
    intermediateResource->SetName(L"IntermediateResource");

    UpdateSubresources(dx12_->GetCommandList(), texture.Get(), intermediateResource.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    dx12_->GetContext().Transition(texture, D3D12_RESOURCE_STATE_GENERIC_READ);

    return intermediateResource;
  }

} // namespace Tako
