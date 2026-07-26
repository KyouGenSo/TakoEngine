#include "RadialBlur.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

  void RadialBlur::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void RadialBlur::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
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
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(1, cBufferResource_->GetGPUVirtualAddress());

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, inputSrvIndex);

    // フルスクリーン三角形描画
    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void RadialBlur::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat2("RadialBlur Center", &cBufferData_->center.x, 0.01f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("RadialBlur Width", &cBufferData_->blurWidth, 0.001f, 0.0f, 1.0f, "%.3f");
    ImGui::DragInt("RadialBlur Sample Count", reinterpret_cast<int*>(&cBufferData_->sampleCount), 1, 1, 1024, "%d");
#endif
  }

  bool RadialBlur::SetGenericParam(const EffectParam& param)
  {
    if (auto* radialBlurParam = std::get_if<RadialBlurParam>(&param)) {
      SetParam(*radialBlurParam);
      return true;
    }
    return false;
  }

  void RadialBlur::SetParam(const RadialBlurParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->center = param.center;
    cBufferData_->blurWidth = param.blurWidth;
    cBufferData_->sampleCount = param.sampleCount;
  }

  void RadialBlur::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0} }, SamplerFilterMode::Linear, SamplerAddressMode::Clamp);
  }

  void RadialBlur::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void RadialBlur::CreateCBV()
  {
    // RadialBlur の定数バッファの生成
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(RadialBlurParam));

    //　map
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // 初期値の設定
    cBufferData_->center = Vector2(0.5f, 0.5f);
    cBufferData_->blurWidth = 0.0f;
    cBufferData_->sampleCount = 8;
  }

} // namespace Tako