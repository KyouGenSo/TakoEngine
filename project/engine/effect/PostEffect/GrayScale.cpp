#include "GrayScale.h"

#include <cassert>

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

namespace Tako {

  void GrayScale::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
  }

  void GrayScale::Apply(uint32_t inputSrvIndex,
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

    // パラメータリソースの設定
    // GrayScale エフェクトではパラメータは使用しないため、設定は行わない

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(kInputTextureParam, inputSrvIndex);

    // フルスクリーン三角形描画
    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void GrayScale::DrawImgui()
  {

  }

  void GrayScale::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0} });
  }

  void GrayScale::CreatePSO()
  {
    BuildFullScreenPSO();
  }

} // namespace Tako