#include "TextureManager.h"
#include "SrvManager.h"
#include "DX12Basic.h"
#include "StringUtility.h"
#include "EnginePaths.h"
#include "Logger.h"
#include "externals/DirectXTex/d3dx12.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <json.hpp>

namespace Tako {

  namespace {

    constexpr const char* kSettingsPath = "resources/Json/TextureSettings.json";

    // エンジンの画像のうち、色でない物
    constexpr std::string_view kEngineDataTextures[] = { "noise0.png", "noise1.png" };

  } // namespace

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

    std::ifstream ifs(kSettingsPath);
    if (!ifs) {
      return;
    }
    try {
      const nlohmann::json root     = nlohmann::json::parse(ifs);
      const nlohmann::json textures = root.value("Textures", nlohmann::json::object());
      for (const auto& item : textures.items()) {
        if (!item.value().value("sRGB", true)) {
          dataTextures_.insert(item.key());
        }
      }
    }
    catch (const nlohmann::json::exception& e) {
      Logger::Log("TextureSettings.json: %s", e.what());
    }
  }

  void TextureManager::Finalize()
  {
    // 全テクスチャの SRV を返却
    for (auto& [name, data] : textureData_) {
      SrvManager::GetInstance()->Free(data.srvIndex);
      SrvManager::GetInstance()->Free(data.rawSrvIndex);
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

    TextureData loaded;
    loaded.fileName = fileName;
    if (!ReadTexture(loaded)) {
      // 毎フレーム引かれても記録は 1 回
      if (failedFiles_.insert(fileName).second) {
        Logger::Log("Texture not found or unreadable: %s", fileName.c_str());
      }
      return;
    }

    TextureData& textureData = textureData_[fileName];
    textureData = std::move(loaded);
    textureData.srvIndex = SrvManager::GetInstance()->Allocate();
    bySrv_[textureData.srvIndex] = &textureData;
    CreateViews(textureData);
  }

  void TextureManager::Unload(const std::string& fileName)
  {
    auto it = textureData_.find(fileName);
    assert(it != textureData_.end() && "TextureManager::Unload: texture not loaded");
    if (it == textureData_.end()) {
      return;
    }

    bySrv_.erase(it->second.srvIndex);
    SrvManager::GetInstance()->Free(it->second.srvIndex);
    SrvManager::GetInstance()->Free(it->second.rawSrvIndex);
    textureData_.erase(it);
  }

  std::string TextureManager::GetFilePath(const std::string& fileName) const
  {
    // エンジン用テクスチャは directoryPath_ を経由せず直接読む
    return fileName.starts_with("EngineResources/") ? fileName : (directoryPath_ + fileName);
  }

  void TextureManager::EndFrame()
  {
    for (auto& [name, data] : textureData_) {
      data.intermediateResource.Reset();
    }

    // GPU は待ち終えていて、次のフレームの描画はまだ積んでいないので、同じ番号のビューを作り直してよい
    for (const std::string& fileName : pendingReloads_) {
      const auto it = textureData_.find(fileName);
      if (it != textureData_.end() && ReadTexture(it->second)) {
        CreateViews(it->second);
      }
    }
    pendingReloads_.clear();
  }

  uint32_t TextureManager::ToRawSRV(uint32_t srvIndex)
  {
    const auto it = bySrv_.find(srvIndex);
    if (it == bySrv_.end() || !DirectX::IsSRGB(it->second->metadata.format)) {
      return srvIndex;
    }
    TextureData& data = *it->second;
    if (data.rawSrvIndex == 0) {
      // 枠が尽きていれば、リニアに直して読むビューのまま使う（暗く出るだけで止めない）
      const uint32_t rawSrvIndex = SrvManager::GetInstance()->Allocate();
      if (rawSrvIndex == SrvManager::kInvalidIndex) {
        return srvIndex;
      }
      data.rawSrvIndex = rawSrvIndex;
      CreateSRV(data.rawSrvIndex, data, DirectX::MakeLinear(data.metadata.format));
    }
    return data.rawSrvIndex;
  }

  bool TextureManager::IsColorTexture(const std::string& fileName) const
  {
    if (fileName.starts_with("EngineResources/")) {
      const std::string_view name = std::string_view(fileName).substr(fileName.find_last_of('/') + 1);
      return std::ranges::find(kEngineDataTextures, name) == std::end(kEngineDataTextures);
    }
    return !dataTextures_.contains(fileName);
  }

  void TextureManager::SetColorTexture(const std::string& fileName, bool isColor)
  {
    if (fileName.starts_with("EngineResources/") || IsColorTexture(fileName) == isColor) {
      return;
    }
    if (isColor) {
      dataTextures_.erase(fileName);
    }
    else {
      dataTextures_.insert(fileName);
    }

    // 既定（色の画像）と違う物だけを書く。std::map の順なので、キーの順に並ぶ
    nlohmann::json textures = nlohmann::json::object();
    for (const std::string& key : dataTextures_) {
      textures[key] = { { "sRGB", false } };
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(kSettingsPath).parent_path(), ec);
    if (std::ofstream ofs(kSettingsPath); ofs) {
      ofs << nlohmann::json{ { "Textures", textures } }.dump(4) << std::endl;
    }
    else {
      Logger::Log("TextureSettings.json: cannot write %s", kSettingsPath);
    }

    // 同じフレームで 2 回読み直すと、1 回目の転送が使う中間バッファを実行前に捨ててしまうので、1 回にまとめる
    if (textureData_.contains(fileName) && std::ranges::find(pendingReloads_, fileName) == pendingReloads_.end()) {
      pendingReloads_.push_back(fileName);
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
    // ImGui はガンマのまま描くので、ファイルの値のまま読むビューを渡す
    const TextureData* data = Acquire(fileName);
    return data ? SrvManager::GetInstance()->GetGPUDescriptorHandle(ToRawSRV(data->srvIndex)) : D3D12_GPU_DESCRIPTOR_HANDLE{};
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

  bool TextureManager::ReadTexture(TextureData& data)
  {
    const std::wstring filePathW = StringUtility::ConvertString(GetFilePath(data.fileName));

    // ファイルに書いてある色空間は使わず、画像ごとの設定（IsColorTexture）で決める
    DirectX::ScratchImage image;
    HRESULT hr = filePathW.ends_with(L".dds")
      ? DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)
      : DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
    if (FAILED(hr)) {
      return false;
    }
    // ponytail: sRGB の形式が無い物（16 bit の PNG・グレースケールなど）は、色の画像でもリニアに直さずに読む（シーンで明るく出る）。要るなら Convert で R8G8B8A8_UNORM_SRGB にする
    const DXGI_FORMAT fileFormat = image.GetMetadata().format;
    image.OverrideFormat(IsColorTexture(data.fileName) ? DirectX::MakeSRGB(fileFormat) : DirectX::MakeLinear(fileFormat));

    // mipmap を生成。_SRGB の形式は、DirectXTex がリニアに直してから縮める
    DirectX::ScratchImage mipImages{};
    if (DirectX::IsCompressed(image.GetMetadata().format)) {
      mipImages = std::move(image);
    }
    else {
      hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 4, mipImages);
      assert(SUCCEEDED(hr));
    }

    // 読み直しのときは、GPU を待った後（EndFrame）なので古いリソースをすぐ捨ててよい
    data.metadata = mipImages.GetMetadata();
    data.resource = MakeTextureResource(data.metadata);
    data.intermediateResource = UploadTextureData(data.resource, mipImages);
    return true;
  }

  void TextureManager::CreateViews(TextureData& data)
  {
    CreateSRV(data.srvIndex, data, data.metadata.format);
    if (data.rawSrvIndex != 0) {
      CreateSRV(data.rawSrvIndex, data, DirectX::MakeLinear(data.metadata.format));
    }
  }

  void TextureManager::CreateSRV(uint32_t srvIndex, const TextureData& data, DXGI_FORMAT format)
  {
    if (data.metadata.IsCubemap()) {
      SrvManager::GetInstance()->CreateSRVForCubeMap(srvIndex, data.resource.Get(), format, UINT_MAX);
    }
    else {
      SrvManager::GetInstance()->CreateSRVForTexture2D(srvIndex, data.resource.Get(), format, static_cast<UINT>(data.metadata.mipLevels));
    }
  }

  GpuResource TextureManager::MakeTextureResource(const DirectX::TexMetadata& metaData)
  {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = static_cast<UINT>(metaData.width);
    resourceDesc.Height = static_cast<UINT>(metaData.height);
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(metaData.arraySize);
    resourceDesc.MipLevels = static_cast<UINT16>(metaData.mipLevels);
    // sRGB の画像は、リニアに直して読むビューと値のまま読むビューの両方を作れるよう、型なしで作る
    resourceDesc.Format = DirectX::IsSRGB(metaData.format) ? DirectX::MakeTypeless(metaData.format) : metaData.format;
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
