#include "CommandContext.h"

#include <cassert>
#include <crtdbg.h>
#include <iterator>
#include <string>

#include "SrvManager.h"

namespace Tako {

  void CommandContext::ShaderBinder::BindCBV(ShaderParamId id, D3D12_GPU_VIRTUAL_ADDRESS address)
  {
    assert(state_.layout && "CommandContext: bind before setting a pipeline");
    const auto parameters = state_.layout->GetParameters();
    for (uint32_t i = 0; i < parameters.size(); ++i) {
      if (parameters[i].id != id) {
        continue;
      }
      assert(parameters[i].kind == ShaderParameter::Kind::Cbv);
      if (isCompute_) {
        context_.commandList_->SetComputeRootConstantBufferView(i, address);
      }
      else {
        context_.commandList_->SetGraphicsRootConstantBufferView(i, address);
      }
      state_.boundMask |= 1ull << i;
    }
  }

  void CommandContext::ShaderBinder::BindSRV(ShaderParamId id, uint32_t srvIndex)
  {
    assert(state_.layout && "CommandContext: bind before setting a pipeline");
    const auto parameters = state_.layout->GetParameters();
    for (uint32_t i = 0; i < parameters.size(); ++i) {
      if (parameters[i].id != id) {
        continue;
      }
      assert(parameters[i].kind != ShaderParameter::Kind::Cbv);
      const D3D12_GPU_DESCRIPTOR_HANDLE handle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);
      if (isCompute_) {
        context_.commandList_->SetComputeRootDescriptorTable(i, handle);
      }
      else {
        context_.commandList_->SetGraphicsRootDescriptorTable(i, handle);
      }
      state_.boundMask |= 1ull << i;
    }
  }

  void CommandContext::Initialize(ID3D12Device* device, PipelineCache* pipelineCache, UploadAllocator* uploadAllocator)
  {
    pipelineCache_ = pipelineCache;
    uploadAllocator_ = uploadAllocator;

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

  void CommandContext::SetRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE* rtv, DXGI_FORMAT rtvFormat, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv, DXGI_FORMAT dsvFormat)
  {
    commandList_->OMSetRenderTargets(rtv ? 1 : 0, rtv, FALSE, dsv);

    const RenderTargetFormats formats{ rtv ? rtvFormat : DXGI_FORMAT_UNKNOWN, dsv ? dsvFormat : DXGI_FORMAT_UNKNOWN };
    if (formats != renderTargetFormats_) {
      renderTargetFormats_ = formats;
      graphics_.psoDirty = true;
    }
  }

  void CommandContext::SetGraphicsPipeline(const GraphicsPipelineDesc& desc)
  {
    if (graphics_.layout && desc == graphicsDesc_) {
      return;
    }
    graphicsDesc_ = desc;
    graphics_.psoDirty = true;

    graphics_.layout = pipelineCache_->GetGraphicsLayout(desc.vs, desc.ps);
    ID3D12RootSignature* rootSignature = graphics_.layout->GetRootSignature();
    if (rootSignature != graphics_.rootSignature) {
      commandList_->SetGraphicsRootSignature(rootSignature);
      graphics_.rootSignature = rootSignature;
      graphics_.boundMask = 0;
    }
  }

  void CommandContext::SetComputePipeline(const ComputePipelineDesc& desc)
  {
    if (compute_.layout && desc == computeDesc_) {
      return;
    }
    computeDesc_ = desc;
    compute_.psoDirty = true;

    compute_.layout = pipelineCache_->GetComputeLayout(desc.cs);
    ID3D12RootSignature* rootSignature = compute_.layout->GetRootSignature();
    if (rootSignature != compute_.rootSignature) {
      commandList_->SetComputeRootSignature(rootSignature);
      compute_.rootSignature = rootSignature;
      compute_.boundMask = 0;
    }
  }

  void CommandContext::DrawInstanced(UINT vertexCount, UINT instanceCount, UINT startVertex, UINT startInstance)
  {
    PrepareDraw();
    commandList_->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
  }

  void CommandContext::DrawIndexedInstanced(UINT indexCount, UINT instanceCount, UINT startIndex, INT baseVertex, UINT startInstance)
  {
    PrepareDraw();
    commandList_->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
  }

  void CommandContext::Dispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ)
  {
    PrepareDispatch();
    commandList_->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
  }

  void CommandContext::ExecuteIndirect(ID3D12CommandSignature* commandSignature, UINT maxCommandCount, GpuResource& argumentBuffer, UINT64 argumentOffset)
  {
    PrepareDraw();
    commandList_->ExecuteIndirect(commandSignature, maxCommandCount, argumentBuffer.Get(), argumentOffset, nullptr, 0);
  }

  void CommandContext::InvalidateState()
  {
    graphicsDesc_ = {};
    computeDesc_ = {};
    graphics_ = {};
    compute_ = {};
    renderTargetFormats_ = {};
    activePso_ = nullptr;
    graphicsPso_ = nullptr;
    computePso_ = nullptr;
    activeTopology_ = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
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

    InvalidateState();
  }

  ID3D12GraphicsCommandList* CommandContext::GetCommandList()
  {
    FlushBarriers();
    return commandList_.Get();
  }

  void CommandContext::PrepareDraw()
  {
    FlushBarriers();
    assert(graphics_.layout && "CommandContext: draw without a graphics pipeline");

    if (graphics_.psoDirty) {
      graphicsPso_ = pipelineCache_->GetGraphicsPipeline(graphicsDesc_, renderTargetFormats_);
      graphics_.psoDirty = false;
    }
    // ディスパッチで PSO の枠をコンピュートに使った後でも、グラフィックスの設定に戻して描く
    if (activePso_ != graphicsPso_) {
      commandList_->SetPipelineState(graphicsPso_);
      activePso_ = graphicsPso_;
    }
    if (activeTopology_ != graphicsDesc_.topology) {
      commandList_->IASetPrimitiveTopology(graphicsDesc_.topology);
      activeTopology_ = graphicsDesc_.topology;
    }
    ValidateBindings(graphics_);
  }

  void CommandContext::PrepareDispatch()
  {
    FlushBarriers();
    assert(compute_.layout && "CommandContext: dispatch without a compute pipeline");

    if (compute_.psoDirty) {
      computePso_ = pipelineCache_->GetComputePipeline(computeDesc_);
      compute_.psoDirty = false;
    }
    if (activePso_ != computePso_) {
      commandList_->SetPipelineState(computePso_);
      activePso_ = computePso_;
    }
    ValidateBindings(compute_);
  }

  void CommandContext::ValidateBindings([[maybe_unused]] const BindState& state) const
  {
#ifdef _DEBUG
    const uint64_t missing = state.layout->GetRequiredMask() & ~state.boundMask;
    if (missing == 0) {
      return;
    }
    std::wstring message = L"CommandContext: unbound shader parameters:";
    const auto parameters = state.layout->GetParameters();
    for (uint32_t i = 0; i < parameters.size(); ++i) {
      if (missing & (1ull << i)) {
        message += L" " + std::wstring(parameters[i].name.begin(), parameters[i].name.end());
      }
    }
    _ASSERT_EXPR(false, message.c_str());
#endif
  }

} // namespace Tako
