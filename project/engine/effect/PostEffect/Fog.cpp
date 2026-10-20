#include "Fog.h"

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "ColorFunc.h"
#include "Renderer.h"
#include "Camera.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "ImGuiManager.h"
#endif

namespace Tako {

  void Fog::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void Fog::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    dx12_->GetContext().Transition(dx12_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    CommandContext::ShaderBinder binder = BeginFullScreenPass(outputRtvHandle, inputSrvIndex);
    binder.BindCBV("gParam"_id, fogParamResource_->GetGPUVirtualAddress());
    binder.BindCBV("gCamera"_id, cameraResource_->GetGPUVirtualAddress());
    binder.BindSRV("gDepthTexture"_id, depthSrvIndex);
    DrawFullScreen();

    dx12_->GetContext().Transition(dx12_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
  }

  void Fog::DrawImgui()
  {
#ifdef _DEBUG
    ImGui::ColorEdit4("FogColor", &fogData_->color.x);
    ImGui::DragFloat("Density", &fogData_->density, 0.001f, 0.0f, 1.0f);
#endif
  }

  bool Fog::SetGenericParam(const EffectParam& param)
  {
    if (auto* fogParam = std::get_if<FogParam>(&param)) {
      SetParam(*fogParam);
      return true;
    }
    return false;
  }

  void Fog::SetParam(const FogParam& param)
  {
    if (fogData_ == nullptr) {
      return;
    }
    fogData_->color = param.color;
    fogData_->density = param.density;
  }

  void Fog::CreateCBV()
  {
    fogParamResource_ = dx12_->MakeBufferResource(sizeof(FogParam));

    // map
    fogParamResource_->Map(0, nullptr, reinterpret_cast<void**>(&fogData_));

    // 初期値を設定
    fogData_->color = Color::kWhite;
    fogData_->density = 0.0f;

    // camera resource の生成--------------------------------------------------------------------------------
    cameraResource_ = dx12_->MakeBufferResource(sizeof(CameraForGPU));

    // map
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    // 初期値を設定
    cameraData_->farPlane = (*Renderer::GetInstance()->GetCamera())->GetFarClip();
    cameraData_->nearPlane = (*Renderer::GetInstance()->GetCamera())->GetNearClip();

    // unmap
    cameraResource_->Unmap(0, nullptr);
  }

} // namespace Tako