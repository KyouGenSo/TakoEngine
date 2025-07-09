#pragma once
#include "IPostEffect.h"
class Dissolve : public IPostEffect
{
public:

  // 初期化
  void Initialize(DX12Basic* dx12, std::string shaderName) override;

  // 描画
  void Apply(
    uint32_t                    inputSrvIndex,   // 入力テクスチャのSRVインデックス
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, // 出力先のRTVハンドル
    uint32_t                    maskSrvIndex,   // 深度バッファが必要なエフェクト用
    Vector4                     clearColor       // 出力先のRTVのクリアカラー
  ) override;

  // Debug描画
  void DrawImgui() override;

  // パラメータ設定
  bool SetGenericParam(const EffectParam& param) override;
  void SetParam(const DissolveParam& param);
  void SetBaseTextureSrvIndex(uint32_t srvIndex){
    baseTexSrvIndex_ = srvIndex;
  }

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV();

private:
  ComPtr<ID3D12Resource> cBufferResource_;
  DissolveParam* cBufferData_ = nullptr;

  uint32_t baseTexSrvIndex_ = 0; // 背景テクスチャのSRVインデックス

};

