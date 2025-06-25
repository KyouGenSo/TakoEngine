#include "IPostEffect.h"

#include <utility>

void IPostEffect::Initialize(DX12Basic* dx12, std::string shaderName)
{
  m_dx12_ = dx12;

  shaderName_ = std::move(shaderName);

  CreateRootSignature();
  CreatePSO();
  CreateCBV();
}

void IPostEffect::DrawImgui()
{
}