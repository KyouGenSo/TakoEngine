#include "WhiteNoise.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "FrameTimer.h"
#include "EnginePaths.h"

#ifdef _DEBUG
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#endif

namespace Tako {

  void WhiteNoise::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
    CreateCBV();
  }

  void WhiteNoise::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, [[maybe_unused]] uint32_t depthSrvIndex, [[maybe_unused]] const Vector4& clearColor)
  {
    cBufferData_->time = FrameTimer::GetInstance()->GetGameTime();

    BeginFullScreenPass(outputRtvHandle, inputSrvIndex)
      .BindCBV("gParam"_id, cBufferResource_->GetGPUVirtualAddress());
    DrawFullScreen();
  }

  void WhiteNoise::DrawImgui()
  {
  }

  bool WhiteNoise::SetGenericParam(const EffectParam& param)
  {
    if (auto* whiteNoiseParam = std::get_if<WhiteNoiseParam>(&param)) {
      SetParam(*whiteNoiseParam);
      return true;
    }
    return false;
  }

  void WhiteNoise::SetParam(const WhiteNoiseParam& param)
  {
    if (cBufferData_ == nullptr) {
      return;
    }
    cBufferData_->time = param.time;
  }

  void WhiteNoise::CreateCBV()
  {
    cBufferResource_ = dx12_->MakeBufferResource(sizeof(WhiteNoiseParam));

    // データの設定
    cBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&cBufferData_));

    // データの初期化
    cBufferData_->time = 0.0f;
  }

} // namespace Tako