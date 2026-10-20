#include "DepthBasedOutline.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "Mat4x4Func.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void DepthBasedOutline::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void DepthBasedOutline::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    dx12_->GetContext().Transition(dx12_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    CommandContext::ShaderBinder binder = BeginFullScreenPass(outputRtvHandle, inputSrvIndex);
    binder.BindCBV("gParam"_id, cBufferResource_->GetGPUVirtualAddress());
    binder.BindSRV("gDepthTexture"_id, depthSrvIndex);
    DrawFullScreen();

    dx12_->GetContext().Transition(dx12_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
  }

  void DepthBasedOutline::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::DragFloat("DepthBasedOutline Thickness", &cBufferData_->outlineThickness, 0.01f, 0.0f, 100.0f, "%.2f");
#endif
  }

  bool DepthBasedOutline::SetGenericParam(const EffectParam& param)
  {
    if (auto* DepthBasedOutlineParam = std::get_if<DepthOutlineParam>(&param)) {
      SetParam(*DepthBasedOutlineParam);
      return true;
    }
    return false;
  }

  void DepthBasedOutline::SetParam(const DepthOutlineParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->outlineThickness = param.outlineThickness;
  }

  void DepthBasedOutline::SetInvProjectionMatrix(const Matrix4x4& invProjectionMatrix)
  {
    cBufferData_->projectionInverse = invProjectionMatrix;
  }

  void DepthBasedOutline::CreateCBV()
  {
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(DepthOutlineParam));

    // map
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // 初期値を設定
    cBufferData_->outlineThickness = 1.0f;
    cBufferData_->projectionInverse = Mat4x4::MakeIdentity();
  }

} // namespace Tako