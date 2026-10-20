#include "NoEffect.h"
#include "DX12Basic.h"
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"
#include <cassert>

#ifdef _DEBUG
#include "DebugUIManager.h"
#endif

namespace Tako {

  void NoEffect::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
  }

  void NoEffect::Apply(uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    [[maybe_unused]] uint32_t depthSrvIndex,
    [[maybe_unused]] const Vector4& clearColor)
  {
    BeginFullScreenPass(outputRtvHandle, inputSrvIndex);
    DrawFullScreen();
  }

  void NoEffect::ApplyToBackBuffer(uint32_t inputSrvIndex)
  {
    // 描画先は SetSwapChain で設定済みのバックバッファ
    CommandContext& context = dx12_->GetContext();
    context.SetGraphicsPipeline(GetPipeline());
    context.Graphics().BindSRV("gTexture"_id, inputSrvIndex);
    DrawFullScreen();
  }

  void NoEffect::DrawImgui()
  {

  }

} // namespace Tako