#include "PostEffectStruct.h"
#include "DX12Basic.h"
#include "RtvManager.h"
#include "SrvManager.h"

namespace Tako {

  void RenderTexture::Create(DX12Basic* dx12, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
  {
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

    SrvManager::GetInstance()->CreateSRVForTexture2D(srvIndex, resource.Get(), format, 1);
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

} // namespace Tako
