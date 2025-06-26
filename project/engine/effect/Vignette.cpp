#include "Vignette.h"

void Vignette::Initialize(DX12Basic* dx12, std::string shaderName)
{
  IPostEffect::Initialize(dx12, shaderName);
}

void Vignette::Apply(uint32_t inputSrvIndex, D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, uint32_t depthSrvIndex, Vector4 clearColor)
{
}

void Vignette::DrawImgui()
{
}

void Vignette::CreateRootSignature()
{
}

void Vignette::CreatePSO()
{
}

void Vignette::CreateCBV()
{
}