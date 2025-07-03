#pragma once
#include "IPostEffect.h"
class NoEffect : public IPostEffect
{
public:

  // 初期化
  void Initialize(DX12Basic* dx12, std::string shaderName) override;

  // 描画
  void Apply(
    uint32_t                    inputSrvIndex,   // 入力テクスチャのSRVインデックス
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, // 出力先のRTVハンドル
    uint32_t                    depthSrvIndex,   // 深度バッファが必要なエフェクト用
    Vector4                     clearColor       // 出力先のRTVのクリアカラー
  ) override;

  void ApplyToBackBuffer(uint32_t inputSrvIndex);

  // Debug描画
  void DrawImgui() override;

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV() override;

};

