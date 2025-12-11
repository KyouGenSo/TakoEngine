#include "IPostEffect.h"

namespace Tako {

void IPostEffect::Initialize(DX12Basic* dx12, const std::string& shaderName)
{
  m_dx12_ = dx12;

  shaderName_ = shaderName;

  CreateRootSignature();
  CreatePSO();
}

} // namespace Tako