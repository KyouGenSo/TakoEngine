#include "GrayScale.h"

#include <cassert>

#include "DX12Basic.h"
#ifdef _DEBUG
#include "DebugUIManager.h"
#endif
#include "SrvManager.h"
#include "StringUtility.h"
#include "EnginePaths.h"

namespace Tako {

  void GrayScale::Initialize(DX12Basic* dx12, const std::string& shaderName)
  {
    IPostEffect::Initialize(dx12, shaderName);
  }

  void GrayScale::Apply(uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    [[maybe_unused]] uint32_t depthSrvIndex,
    [[maybe_unused]] const Vector4& clearColor)
  {
    BeginFullScreenPass(outputRtvHandle, inputSrvIndex);
    DrawFullScreen();
  }

  void GrayScale::DrawImgui()
  {

  }

} // namespace Tako