#include "NoEffect.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"
#include <cassert>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  void NoEffect::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
  }

  void NoEffect::Apply(uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    [[maybe_unused]] uint32_t depthSrvIndex,
    [[maybe_unused]] const Vector4& clearColor)
  {
    dx12_->GetCommandList()->OMSetRenderTargets(1,
      &outputRtvHandle,
      false,
      nullptr);


    // エフェクト適用シェーダーの設定
    dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジーの設定（フルスクリーン三角形用）
    dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void NoEffect::ApplyToBackBuffer(uint32_t inputSrvIndex)
  {
    // エフェクト適用シェーダーの設定
    dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジーの設定（フルスクリーン三角形用）
    dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void NoEffect::DrawImgui()
  {

  }

  void NoEffect::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0} });
  }

  void NoEffect::CreatePSO()
  {
    BuildFullScreenPSO();
  }

} // namespace Tako