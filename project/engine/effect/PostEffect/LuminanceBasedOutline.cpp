#include "LuminanceBasedOutline.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

  void LuminanceBasedOutline::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void LuminanceBasedOutline::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dx12_->GetDSVHeapHandleStart();
    dx12_->GetCommandList()->OMSetRenderTargets(1,
      &outputRtvHandle,
      false,
      &dsvHandle);

    // エフェクト適用シェーダーの設定
    dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジーの設定（フルスクリーン三角形用）
    dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // パラメータリソースの設定
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource_->GetGPUVirtualAddress());

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void LuminanceBasedOutline::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("LuminanceBasedOutline Thickness", &cBufferData_->outlineThickness, 0.01f, 0.0f, 100.0f, "%.2f");
#endif
  }

  bool LuminanceBasedOutline::SetGenericParam(const EffectParam& param)
  {
    if (auto* luminanceBasedOutlineParam = std::get_if<LuminanceOutlineParam>(&param)) {
      SetParam(*luminanceBasedOutlineParam);
      return true;
    }
    return false;
  }

  void LuminanceBasedOutline::SetParam(const LuminanceOutlineParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->outlineThickness = param.outlineThickness;
  }

  void LuminanceBasedOutline::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0} });
  }

  void LuminanceBasedOutline::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void LuminanceBasedOutline::CreateCBV()
  {
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(LuminanceOutlineParam));

    // map
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // 初期値を設定
    cBufferData_->outlineThickness = 1.0f;
  }

} // namespace Tako