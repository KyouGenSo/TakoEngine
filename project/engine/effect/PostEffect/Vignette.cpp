#include "Vignette.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#include <cassert>

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void Vignette::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void Vignette::Apply(const uint32_t inputSrvIndex, const D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] const uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    BeginFullScreenPass(outputRtvHandle, inputSrvIndex)
      .BindCBV("gParam"_id, cBufferResource_->GetGPUVirtualAddress());
    DrawFullScreen();
  }

  void Vignette::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("Vignette Power", &cBufferData_->power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Vignette Range", &cBufferData_->range, 0.01f, 0.0f, 100.0f);
    ImGui::ColorEdit3("Vignette Color", &cBufferData_->color.x);
#endif
  }

  bool Vignette::SetGenericParam(const EffectParam& param)
  {
    if (auto* vignetteParam = std::get_if<VignetteParam>(&param)) {
      SetParam(*vignetteParam);
      return true;
    }
    return false;
  }

  void Vignette::SetParam(const VignetteParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->power = param.power;
    cBufferData_->range = param.range;
    cBufferData_->color.x = param.color.x;
    cBufferData_->color.y = param.color.y;
    cBufferData_->color.z = param.color.z;
  }

  void Vignette::CreateCBV()
  {
    // VignetteParam のリソース生成
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(VignetteParam));

    // データの設定
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // データの初期化
    cBufferData_->power = 0.0f;
    cBufferData_->range = 20.0f;
    cBufferData_->color = Vector3(0.0f, 0.0f, 0.0f);
  }

} // namespace Tako