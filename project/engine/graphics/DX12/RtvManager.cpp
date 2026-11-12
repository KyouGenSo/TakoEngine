#include"RtvManager.h"
#include"DX12Basic.h"
#include <cassert>

namespace Tako {

  std::unique_ptr<RtvManager> RtvManager::instance_ = nullptr;

  RtvManager* RtvManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<RtvManager>(Token{});
    }
    return instance_.get();
  }

  void RtvManager::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    heap_.Initialize(dx12_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kMaxRTVCount, false);
  }

  void RtvManager::Finalize()
  {
    assert(heap_.GetAllocatedCount() == 0 && "RTV indices leaked");

    heap_.Finalize();
    dx12_ = nullptr;
  }

  uint32_t RtvManager::Allocate()
  {
    return heap_.Allocate();
  }

  void RtvManager::Free(uint32_t rtvIndex)
  {
    heap_.Free(rtvIndex);
  }

  void RtvManager::CreateRTV(uint32_t rtvIndex, ID3D12Resource* pResource, DXGI_FORMAT format)
  {
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    dx12_->GetDevice()->CreateRenderTargetView(pResource, &rtvDesc, heap_.GetCpuHandle(rtvIndex));
  }

  bool RtvManager::IsAllocated(uint32_t rtvIndex) const
  {
    return heap_.IsAllocated(rtvIndex);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE RtvManager::GetCpuHandle(uint32_t rtvIndex) const
  {
    return heap_.GetCpuHandle(rtvIndex);
  }

} // namespace Tako
