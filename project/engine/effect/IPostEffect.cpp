#include "IPostEffect.h"

void IPostEffect::Initialize(DX12Basic* dx12, std::string shaderName)
{
  m_dx12_ = dx12;

  shaderName_ = shaderName;

  CreateRootSignature();
  CreatePSO();
  CreateCBV();
}