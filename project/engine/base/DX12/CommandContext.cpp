#include "CommandContext.h"

#include <cassert>
#include <iterator>

namespace Tako {

  void CommandContext::Initialize(ID3D12Device* device)
  {
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_));
    assert(SUCCEEDED(hr));

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
  }

  void CommandContext::Transition(GpuResource& resource, D3D12_RESOURCE_STATES newState)
  {
    if (resource.state == newState) {
      return;
    }

    // このリソースへの最後の未発行バリアが遷移なら、遷移先だけ差し替えて 1 本にまとめる
    for (auto it = pendingBarriers_.rbegin(); it != pendingBarriers_.rend(); ++it) {
      const bool isSameResource = (it->Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) ? it->Transition.pResource == resource.Get()
                                                                                       : it->UAV.pResource == resource.Get();
      if (!isSameResource) {
        continue;
      }
      if (it->Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
        it->Transition.StateAfter = newState;
        if (it->Transition.StateBefore == newState) {
          pendingBarriers_.erase(std::next(it).base());
        }
        resource.state = newState;
        return;
      }
      break;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = resource.state;
    barrier.Transition.StateAfter = newState;
    pendingBarriers_.push_back(barrier);

    resource.state = newState;
  }

  void CommandContext::UavBarrier(GpuResource& resource)
  {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource.Get();
    pendingBarriers_.push_back(barrier);
  }

  void CommandContext::FlushBarriers()
  {
    if (pendingBarriers_.empty()) {
      return;
    }
    commandList_->ResourceBarrier(static_cast<UINT>(pendingBarriers_.size()), pendingBarriers_.data());
    pendingBarriers_.clear();
  }

  void CommandContext::Dispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ)
  {
    GetCommandList()->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
  }

  void CommandContext::ExecuteIndirect(ID3D12CommandSignature* commandSignature, UINT maxCommandCount, GpuResource& argumentBuffer, UINT64 argumentOffset)
  {
    GetCommandList()->ExecuteIndirect(commandSignature, maxCommandCount, argumentBuffer.Get(), argumentOffset, nullptr, 0);
  }

  void CommandContext::Close()
  {
    FlushBarriers();
    [[maybe_unused]] HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));
  }

  void CommandContext::Reset()
  {
    HRESULT hr = allocator_->Reset();
    assert(SUCCEEDED(hr));

    hr = commandList_->Reset(allocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
  }

  ID3D12GraphicsCommandList* CommandContext::GetCommandList()
  {
    FlushBarriers();
    return commandList_.Get();
  }

} // namespace Tako
