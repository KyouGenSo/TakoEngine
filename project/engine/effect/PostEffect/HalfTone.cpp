#include "HalfTone.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "WinApp.h"
#include "EnginePaths.h"

#include <cassert>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void HalfTone::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void HalfTone::Apply(const uint32_t inputSrvIndex, const D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] const uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    // スクリーンサイズを更新
    UpdateScreenSize();

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

  void HalfTone::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("Dot Size", &cBufferData_->dotSize, 1.0f, 1.0f, 50.0f);
    ImGui::DragFloat("Contrast", &cBufferData_->contrast, 0.01f, 0.1f, 3.0f);
    ImGui::DragFloat("Angle", &cBufferData_->angle, 0.01f, 0.0f, 6.28f);
    ImGui::DragFloat("Threshold", &cBufferData_->threshold, 0.01f, 0.0f, 1.0f);

    // ドットパターン選択
    const char* patterns[] = { "Circle", "Square", "Diamond" };
    ImGui::Combo("Dot Pattern", &cBufferData_->dotPattern, patterns, 3);

    // カラーモード選択
    const char* colorModes[] = { "Monochrome", "CMYK" };
    ImGui::Combo("Color Mode", &cBufferData_->colorMode, colorModes, 2);

    ImGui::Text("Screen Size: %.0f x %.0f", cBufferData_->screenSize.x, cBufferData_->screenSize.y);
#endif
  }

  bool HalfTone::SetGenericParam(const EffectParam& param)
  {
    if (auto* halfToneParam = std::get_if<HalfToneParam>(&param)) {
      SetParam(*halfToneParam);
      return true;
    }
    return false;
  }

  void HalfTone::SetParam(const HalfToneParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->dotSize = param.dotSize;
    cBufferData_->contrast = param.contrast;
    cBufferData_->angle = param.angle;
    cBufferData_->dotPattern = param.dotPattern;
    cBufferData_->colorMode = param.colorMode;
    cBufferData_->threshold = param.threshold;
    // screenSize は自動更新されるため、ここでは設定しない
  }

  void HalfTone::UpdateScreenSize()
  {
    if (cBufferData_ != nullptr) {
      cBufferData_->screenSize.x = static_cast<float>(WinApp::clientWidth);
      cBufferData_->screenSize.y = static_cast<float>(WinApp::clientHeight);
    }
  }

  void HalfTone::CreateRootSignature()
  {
    BuildRootSignature({ {RootParam::SrvTable, 0}, {RootParam::Cbv, 0} });
  }

  void HalfTone::CreatePSO()
  {
    BuildFullScreenPSO();
  }

  void HalfTone::CreateCBV()
  {
    // HalfToneParam のリソース生成
    cBufferResource_ = m_dx12_->MakeBufferResource(sizeof(HalfToneParam));

    // データの設定
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // データの初期化
    cBufferData_->dotSize = 8.0f;
    cBufferData_->contrast = 1.0f;
    cBufferData_->angle = 0.0f;
    cBufferData_->dotPattern = 0;       // 円
    cBufferData_->colorMode = 0;        // モノクロ
    cBufferData_->threshold = 0.0f;
    cBufferData_->padding = 0.0f;

    // スクリーンサイズを WinApp から取得
    UpdateScreenSize();
  }

} // namespace Tako