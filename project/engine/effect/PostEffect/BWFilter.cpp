#include "BWFilter.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void BWFilter::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void BWFilter::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    BeginFullScreenPass(outputRtvHandle, inputSrvIndex)
      .BindCBV("gParam"_id, cBufferResource_->GetGPUVirtualAddress());
    DrawFullScreen();
  }

  void BWFilter::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("BWFilter Threshold", &cBufferData_->threshold, 0.01f, 0.0f, 1.0f, "%.2f");
#endif
  }

  bool BWFilter::SetGenericParam(const EffectParam& param)
  {
    if (auto* bwFilterParam = std::get_if<BWFilterParam>(&param)) {
      SetParam(*bwFilterParam);
      return true;
    }
    return false;
  }

  void BWFilter::SetParam(const BWFilterParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->threshold = param.threshold;
  }

  void BWFilter::CreateCBV()
  {
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(BWFilterParam));

    // map
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // 初期値の設定
    cBufferData_->threshold = 0.5f;
  }

} // namespace Tako