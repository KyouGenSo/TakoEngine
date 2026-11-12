#include"DsvManager.h"
#include"DX12Basic.h"
#include <cassert>

namespace Tako {

  std::unique_ptr<DsvManager> DsvManager::instance_ = nullptr;

  DsvManager* DsvManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<DsvManager>(Token{});
    }
    return instance_.get();
  }

  void DsvManager::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    heap_.Initialize(dx12_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kMaxDSVCount, false);
  }

  void DsvManager::Finalize()
  {
    assert(heap_.GetAllocatedCount() == 0 && "DSV indices leaked");

    heap_.Finalize();
    dx12_ = nullptr;
  }

  uint32_t DsvManager::Allocate()
  {
    return heap_.Allocate();
  }

  void DsvManager::Free(uint32_t dsvIndex)
  {
    heap_.Free(dsvIndex);
  }

  void DsvManager::CreateDSV(uint32_t dsvIndex, ID3D12Resource* pResource, DXGI_FORMAT format)
  {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    dx12_->GetDevice()->CreateDepthStencilView(pResource, &dsvDesc, heap_.GetCpuHandle(dsvIndex));
  }

  bool DsvManager::IsAllocated(uint32_t dsvIndex) const
  {
    return heap_.IsAllocated(dsvIndex);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE DsvManager::GetCpuHandle(uint32_t dsvIndex) const
  {
    return heap_.GetCpuHandle(dsvIndex);
  }

} // namespace Tako
