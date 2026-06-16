#include "RGBSplit.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

  void RGBSplit::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void RGBSplit::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dx12_->GetDSVHeapHandleStart();
    m_dx12_->GetCommandList()->OMSetRenderTargets(1,
      &outputRtvHandle,
      false,
      &dsvHandle);

    // エフェクト適用シェーダーの設定
    m_dx12_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    m_dx12_->GetCommandList()->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジーの設定（フルスクリーン三角形用）
    m_dx12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // パラメータリソースの設定
    m_dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource_->GetGPUVirtualAddress());

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    m_dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void RGBSplit::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat2("Red Offset", &cBufferData_->redOffset.x, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat2("Green Offset", &cBufferData_->greenOffset.x, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat2("Blue Offset", &cBufferData_->blueOffset.x, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("RGBSplit Intensity", &cBufferData_->intensity, 0.01f, 0.0f, 1.0f);
#endif
  }

  bool RGBSplit::SetGenericParam(const EffectParam& param)
  {
    if (auto* rgbSplitParam = std::get_if<RGBSplitParam>(&param)) {
      SetParam(*rgbSplitParam);
      return true;
    }
    return false;
  }

  void RGBSplit::SetParam(const RGBSplitParam& param)
  {
    if (cBufferData_ == nullptr) {
      return; // cBufferData_が初期化されていない場合は何もしない
    }
    // パラメータを設定
    cBufferData_->redOffset = param.redOffset;
    cBufferData_->greenOffset = param.greenOffset;
    cBufferData_->blueOffset = param.blueOffset;
    cBufferData_->intensity = param.intensity;
  }

  void RGBSplit::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0} }, SamplerFilterMode::Linear, SamplerAddressMode::Clamp);
  }

  void RGBSplit::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void RGBSplit::CreateCBV()
  {
    // RadialBlur の定数バッファの生成
    cBufferResource_ = m_dx12_->MakeBufferResource(sizeof(RGBSplitParam));

    //　map
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // 初期化
    cBufferData_->redOffset = Vector2(0.0f, 0.0f);
    cBufferData_->greenOffset = Vector2(0.0f, 0.0f);
    cBufferData_->blueOffset = Vector2(0.0f, 0.0f);
    cBufferData_->intensity = 0.0f;
  }

} // namespace Tako