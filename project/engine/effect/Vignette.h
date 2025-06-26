#pragma once
#include "IPostEffect.h"
#include "PostEffectStruct.h"

class Vignette : public IPostEffect
{
public:

  // 初期化
  void Initialize(DX12Basic* dx12, std::string shaderName) override;

  // 描画
  void Apply(
    uint32_t inputSrvIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
    uint32_t depthSrvIndex, // 深度バッファが必要なエフェクト用
    Vector4 clearColor
  ) override;

  // Debug描画
  void DrawImgui() override;

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV() override;

private:

  ComPtr<ID3D12Resource> cBufferResource_;
  VignetteParam* cBufferData_ = nullptr;
};

