#pragma once
#include "IPostEffect.h"

class RadialBlur : public IPostEffect
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
  void SetParam(const RadialBlurParam& param);

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV() override;

private:

  ComPtr<ID3D12Resource> cBufferResource_;
  RadialBlurParam* cBufferData_ = nullptr;

  Vector2 center_ = { .x = 0.5f, .y = 0.5f }; // 中心位置
  float blurWidth_ = 0.0f;                // ブラーの幅
  int32_t sampleCount_ = 8;              // サンプル数
};

