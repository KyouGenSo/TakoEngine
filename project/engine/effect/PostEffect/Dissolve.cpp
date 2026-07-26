#include "Dissolve.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void Dissolve::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();

    baseTexSrvIndex_ = TextureManager::GetInstance()->GetEngineDefaultSRVIndex("black.png");
  }

  void Dissolve::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t maskSrvIndex, [[maybe_unused]] const Vector4& clearColor)
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
    dx12_->GetCommandList()->SetGraphicsRootConstantBufferView(kParameterCbvParam, cBufferResource_->GetGPUVirtualAddress());

    // レンダーテクスチャ A をシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(kInputTextureParam, inputSrvIndex);

    // マスクテクスチャをシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(kMaskTextureParam, maskSrvIndex);

    // ベーステクスチャをシェーダーリソースとして設定
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(kBaseTextureParam, baseTexSrvIndex_);

    // フルスクリーン三角形描画
    dx12_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
  }

  void Dissolve::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("Dissolve Threshold", &cBufferData_->threshold, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Edge Thickness", &cBufferData_->edgeThickness, 0.01f, 0.0f, 1.0f);
    ImGui::ColorEdit4("Edge Color", &cBufferData_->edgeColor.x);
#endif
  }

  bool Dissolve::SetGenericParam(const EffectParam& param)
  {
    if (auto* dissolveParam = std::get_if<DissolveParam>(&param)) {
      SetParam(*dissolveParam);
      return true;
    }
    return false;
  }

  void Dissolve::SetParam(const DissolveParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->threshold = param.threshold;
    cBufferData_->edgeThickness = param.edgeThickness;
    cBufferData_->edgeColor = param.edgeColor;
  }

  void Dissolve::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0}, {RootParam::SrvTable, 1}, {RootParam::SrvTable, 2} });
  }

  void Dissolve::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void Dissolve::CreateCBV()
  {
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(DissolveParam));

    // データの設定
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // データの初期化
    cBufferData_->threshold = 0.0f; // デフォルト値を設定
    cBufferData_->edgeThickness = 0.0f;
    cBufferData_->edgeColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f); // デフォルト値を設定
  }

} // namespace Tako