#include "FXAA.h"

#include "DX12Basic.h"
#include "SrvManager.h"

namespace Tako {

  void FXAA::Apply(uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    [[maybe_unused]] uint32_t depthSrvIndex,
    [[maybe_unused]] const Vector4& clearColor)
  {
    BeginFullScreenPass(outputRtvHandle, inputSrvIndex);
    DrawFullScreen();
  }

} // namespace Tako
