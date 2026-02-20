#include"SrvManager.h"
#include"DX12Basic.h"

namespace Tako {

  std::unique_ptr<SrvManager> SrvManager::instance_ = nullptr;

  const uint32_t SrvManager::kMaxSRVCount = 2048;

  SrvManager* SrvManager::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<SrvManager>(new SrvManager());
    }
    return instance_.get();
  }

  void SrvManager::Initialize(DX12Basic* dx12)
  {
    m_dx12_ = dx12;

    // SRV のディスクリプタのサイズを取得
    descriptorSize_ = m_dx12_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // SRV のディスクリプタヒープの生成
    descriptorHeap_ = m_dx12_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);


    nextNewIndex_ = 0;
    allocatedCount_ = 0;
    usedIndices_.clear();
  }

  void SrvManager::Finalize()
  {
    instance_.reset();
  }

  void SrvManager::BeginDraw()
  {
    // SRV のディスクリプタヒープをセット
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { descriptorHeap_.Get() };
    m_dx12_->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps->GetAddressOf());
  }

  uint32_t SrvManager::Allocate()
  {
    uint32_t index;

    // フリーリストから優先的に取得
    if (!freeIndices_.empty()) {
      index = freeIndices_.top();
      freeIndices_.pop();
    }
    else {
      // フリーリストが空の場合は新しいインデックスを使用
      if (nextNewIndex_ >= kMaxSRVCount) {
        assert(false && "SRV index limit reached");
        return UINT32_MAX; // エラー値
      }
      index = nextNewIndex_++;
    }

    // 使用中として記録
    usedIndices_.insert(index);
    allocatedCount_++;

    return index;
  }

  void SrvManager::Free(uint32_t index)
  {
    // 無効なインデックスのチェック
    if (index >= kMaxSRVCount) {
      assert(false && "Invalid SRV index");
      return;
    }

    // 使用中かチェック
    auto it = usedIndices_.find(index);
    if (it == usedIndices_.end()) {
      assert(false && "Trying to free an SRV index that is not allocated");
      return;
    }

    // 使用中リストから削除
    usedIndices_.erase(it);
    allocatedCount_--;

    // フリーリストに追加
    freeIndices_.push(index);
  }

  bool SrvManager::CanAllocate()
  {
    // フリーリストに空きがあるか、新しいインデックスが使えるかチェック
    return !freeIndices_.empty() || nextNewIndex_ < kMaxSRVCount;
  }

  bool SrvManager::IsAllocated(uint32_t index) const
  {
    return usedIndices_.find(index) != usedIndices_.end();
  }

  void SrvManager::CreateSRVForTexture2D(uint32_t index, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = mipLevels;

    m_dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(index));
  }

  void SrvManager::CreateSRVForStructuredBuffer(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    m_dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(index));
  }

  void SrvManager::CreateUAV(uint32_t index, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.StructureByteStride = structureByteStride;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    m_dx12_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(index));
  }

  void SrvManager::CreateSRVForCubeMap(uint32_t _srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    m_dx12_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(_srvIndex));
  }

  void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t index)
  {
    m_dx12_->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(index));
  }

  void SrvManager::SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t index)
  {
    m_dx12_->GetCommandList()->SetComputeRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(index));
  }

  D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += descriptorSize_ * index;

    return handleCPU;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += descriptorSize_ * index;

    return handleGPU;
  }

} // namespace Tako
