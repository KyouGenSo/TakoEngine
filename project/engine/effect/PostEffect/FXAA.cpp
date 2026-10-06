#include "FXAA.h"

#include "DX12Basic.h"
#include "SrvManager.h"

namespace Tako {

  void FXAA::Apply(uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    [[maybe_unused]] uint32_t depthSrvIndex,
    [[maybe_unused]] const Vector4& clearColor)
  {
    ID3D12GraphicsCommandList* commandList = dx12_->GetCommandList();
    commandList->OMSetRenderTargets(1, &outputRtvHandle, false, nullptr);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(kInputTextureParam, inputSrvIndex);
    commandList->DrawInstanced(3, 1, 0, 0);
  }

  void FXAA::CreateRootSignature()
  {
    // 画面端で反対側を拾わないよう Clamp
    BuildRootSignature({ {RootParam::SrvTable, 0} }, SamplerFilterMode::Linear, SamplerAddressMode::Clamp);
  }

  void FXAA::CreatePSO()
  {
    BuildFullScreenPSO();
  }

} // namespace Tako
