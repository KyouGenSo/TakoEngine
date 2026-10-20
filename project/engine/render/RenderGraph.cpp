#include "RenderGraph.h"

#include <algorithm>

#include "DX12Basic.h"
#include "RtvManager.h"
#include "SrvManager.h"

namespace Tako {

  void RenderTexture::Create(DX12Basic* dx12, uint32_t textureWidth, uint32_t textureHeight, DXGI_FORMAT textureFormat, const Vector4& clearColor, bool forceOpaqueAlpha)
  {
    width = textureWidth;
    height = textureHeight;
    format = textureFormat;
    dx12->CreateRenderTextureResource(resource, width, height, format, clearColor);

    // Release を挟まない再作成でも二重確保しないよう、未確保時のみインデックスを取得
    if (rtvIndex == RtvManager::kInvalidIndex) {
      rtvIndex = RtvManager::GetInstance()->Allocate();
    }
    if (srvIndex == SrvManager::kInvalidIndex) {
      srvIndex = SrvManager::GetInstance()->Allocate();
    }

    RtvManager::GetInstance()->CreateRTV(rtvIndex, resource.Get(), format);
    rtvHandle = RtvManager::GetInstance()->GetCpuHandle(rtvIndex);

    SrvManager::GetInstance()->CreateSRVForTexture2D(srvIndex, resource.Get(), format, 1, forceOpaqueAlpha);
  }

  void RenderTexture::Release()
  {
    resource.Reset();

    RtvManager::GetInstance()->Free(rtvIndex);
    rtvIndex = RtvManager::kInvalidIndex;

    SrvManager::GetInstance()->Free(srvIndex);
    srvIndex = SrvManager::kInvalidIndex;

    rtvHandle = {};
  }

  void RenderGraph::Finalize()
  {
    for (Entry& entry : pool_) {
      entry.texture->Release();
    }
    pool_.clear();
  }

  void RenderGraph::BeginFrame()
  {
    // 解像度が変わった後の古い大きさの RT や、閉じたプレビューの RT はここで消える
    std::erase_if(pool_, [](Entry& entry) {
      if (!entry.used) {
        entry.texture->Release();
        return true;
      }
      return false;
    });
    for (Entry& entry : pool_) {
      entry.inUse = false;
      entry.used = false;
    }
  }

  RenderTexture& RenderGraph::Acquire(const TextureDesc& desc)
  {
    for (Entry& entry : pool_) {
      if (!entry.inUse && entry.desc == desc) {
        entry.inUse = true;
        entry.used = true;
        return *entry.texture;
      }
    }

    Entry& entry = pool_.emplace_back(desc, std::make_unique<RenderTexture>(), true, true);
    entry.texture->Create(dx12_, desc.width, desc.height, desc.format, desc.clearColor);
    entry.texture->resource->SetName(L"RenderGraphTexture");
    return *entry.texture;
  }

  void RenderGraph::Release(const RenderTexture& texture)
  {
    const auto it = std::ranges::find_if(pool_, [&texture](const Entry& entry) { return entry.texture.get() == &texture; });
    if (it != pool_.end()) {
      it->inUse = false;
    }
  }

  void RenderGraph::AddPass(std::initializer_list<Access> accesses, const std::function<void()>& execute)
  {
    CommandContext& context = dx12_->GetContext();
    for (const Access& access : accesses) {
      context.Transition(access.resource, access.state);
    }
    execute();
  }

} // namespace Tako
