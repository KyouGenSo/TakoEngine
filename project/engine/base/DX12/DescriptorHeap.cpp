#include "DescriptorHeap.h"
#include <cassert>

namespace Tako {

  void DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible)
  {
    assert(device);
    assert(numDescriptors > 1 && "index 0 is reserved as the invalid sentinel");

    // ヒープの設定
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Type = type;
    heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    // ヒープの生成
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap_));
    assert(SUCCEEDED(hr));

    descriptorSize_ = device->GetDescriptorHandleIncrementSize(type);
    numDescriptors_ = numDescriptors;
    shaderVisible_ = shaderVisible;
    cpuStart_ = heap_->GetCPUDescriptorHandleForHeapStart();
    gpuStart_ = shaderVisible ? heap_->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};

    // 再初期化時に前世代の割り当て状態が残らないよう全リセット
    freeIndices_ = {};
    usedIndices_.clear();
    nextNewIndex_ = 1;
    allocatedCount_ = 0;
  }

  void DescriptorHeap::Finalize()
  {
    heap_.Reset();
    cpuStart_ = {};
    gpuStart_ = {};
    descriptorSize_ = 0;
    numDescriptors_ = 0;
    shaderVisible_ = false;
    freeIndices_ = {};
    usedIndices_.clear();
    nextNewIndex_ = 1;
    allocatedCount_ = 0;
  }

  uint32_t DescriptorHeap::Allocate()
  {
    uint32_t index;

    // フリーリストから優先的に取得
    if (!freeIndices_.empty()) {
      index = freeIndices_.top();
      freeIndices_.pop();
    }
    else {
      // フリーリストが空の場合は新しいインデックスを使用
      if (nextNewIndex_ >= numDescriptors_) {
        assert(false && "descriptor heap index limit reached");
        return kInvalidIndex;
      }
      index = nextNewIndex_++;
    }

    usedIndices_.insert(index);
    allocatedCount_++;

    return index;
  }

  void DescriptorHeap::Free(uint32_t index)
  {
    // 番兵と Finalize 後の解放は正当な no-op（未設定メンバや終了時のデストラクタから呼ばれる）
    if (index == kInvalidIndex || !heap_) {
      return;
    }

    if (index >= numDescriptors_) {
      assert(false && "invalid descriptor index");
      return;
    }

    auto it = usedIndices_.find(index);
    if (it == usedIndices_.end()) {
      assert(false && "trying to free a descriptor index that is not allocated");
      return;
    }

    usedIndices_.erase(it);
    allocatedCount_--;

    freeIndices_.push(index);
  }

  bool DescriptorHeap::CanAllocate() const
  {
    return !freeIndices_.empty() || nextNewIndex_ < numDescriptors_;
  }

  bool DescriptorHeap::IsAllocated(uint32_t index) const
  {
    return usedIndices_.find(index) != usedIndices_.end();
  }

  D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandle(uint32_t index) const
  {
    assert(heap_ && index < numDescriptors_);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart_;
    handle.ptr += static_cast<SIZE_T>(descriptorSize_) * index;

    return handle;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandle(uint32_t index) const
  {
    assert(heap_ && shaderVisible_ && index < numDescriptors_);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart_;
    handle.ptr += static_cast<UINT64>(descriptorSize_) * index;

    return handle;
  }

} // namespace Tako
