#include"SrvManager.h"
#include"DX12Basic.h"
#include <cassert>
#include <cstdio>
#ifdef _DEBUG
#include <Windows.h>
#endif

namespace Tako {

  std::unique_ptr<SrvManager> SrvManager::instance_ = nullptr;

  SrvManager* SrvManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<SrvManager>(Token{});
    }
    return instance_.get();
  }

  void SrvManager::Initialize(DX12Basic* dx12)
  {
    dx12_ = dx12;

    heap_.Initialize(dx12_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
  }

  void SrvManager::Finalize()
  {
#ifdef _DEBUG
    // 解放漏れの検出。利用側の Free 忘れをここで可視化する
    if (heap_.GetAllocatedCount() > 0) {
      char buf[64];
      snprintf(buf, sizeof(buf), "SrvManager: %u SRV indices leaked:\n", heap_.GetAllocatedCount());
      OutputDebugStringA(buf);
      for (uint32_t index : heap_.GetUsedIndices()) {
        snprintf(buf, sizeof(buf), "  srvIndex %u\n", index);
        OutputDebugStringA(buf);
      }
    }
#endif
    assert(heap_.GetAllocatedCount() == 0 && "SRV indices leaked");

    // インスタンスは温存し、以後の GetInstance() を有効なまま Free() を no-op にする
    heap_.Finalize();
    dx12_ = nullptr;
  }

  void SrvManager::BeginDraw()
  {
    ID3D12DescriptorHeap* descriptorHeaps[] = { heap_.GetHeap() };
    dx12_->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
  }

  uint32_t SrvManager::Allocate()
  {
    return heap_.Allocate();
  }

  void SrvManager::Free(uint32_t srvIndex)
  {
    heap_.Free(srvIndex);
  }

  bool SrvManager::CanAllocate() const
  {
    return heap_.CanAllocate();
  }

  bool SrvManager::IsAllocated(uint32_t srvIndex) const
  {
    return heap_.IsAllocated(srvIndex);
  }

  void SrvManager::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels, bool forceOpaqueAlpha)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    // RGB はメモリのまま、A のみ定数 1.0 を返す
    srvDesc.Shader4ComponentMapping = forceOpaqueAlpha
      ? D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
          D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
          D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
          D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
          D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1)
      : D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = mipLevels;

    dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
  }

  void SrvManager::CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
  }

  void SrvManager::CreateUAV(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.StructureByteStride = structureByteStride;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    dx12_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(srvIndex));
  }

  void SrvManager::CreateSRVForCubeMap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
  }

  void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex)
  {
    dx12_->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
  }

  void SrvManager::SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex)
  {
    dx12_->GetCommandList()->SetComputeRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
  }

  D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t srvIndex) const
  {
    return heap_.GetCpuHandle(srvIndex);
  }

  D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t srvIndex) const
  {
    return heap_.GetGpuHandle(srvIndex);
  }

} // namespace Tako
