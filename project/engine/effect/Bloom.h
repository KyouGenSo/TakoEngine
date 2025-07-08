#pragma once
#include "IPostEffect.h"

class Bloom : public IPostEffect
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

  // Debug描画
  void DrawImgui() override;

  // パラメータ設定
  bool SetGenericParam(const EffectParam& param) override;
  void SetParam(const BloomParam& param);

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV();
  void CreateRenderTexture();

  void SetBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

private:

  ComPtr<ID3D12Resource> cBufferResource1_;
  BloomParam* cBufferData1_ = nullptr;

  ComPtr<ID3D12Resource> cBufferResource2_;
  BloomParam* cBufferData2_ = nullptr;

  RenderTexture resultRT_;
};

