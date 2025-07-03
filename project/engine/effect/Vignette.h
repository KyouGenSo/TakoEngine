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
    uint32_t                    inputSrvIndex,   // 入力テクスチャのSRVインデックス
    D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle, // 出力先のRTVハンドル
    uint32_t                    depthSrvIndex,   // 深度バッファが必要なエフェクト用
    Vector4                     clearColor       // 出力先のRTVのクリアカラー
  ) override;

  // Debug描画
  void DrawImgui() override;

  // パラメータ設定
  bool SetGenericParam(const EffectParam& param) override;
  void SetParam(const VignetteParam& param);
  void SetPower(float power);
  void SetRange(float range);
  void SetColor(const Vector3& color);

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV() override;

private:

  ComPtr<ID3D12Resource> cBufferResource_;
  VignetteParam* cBufferData_ = nullptr;

  float power_ = 0.5f; // 効果の強さ
  float range_ = 0.0f; // 効果の範囲
  Vector3 color_{ 0.0f, 0.0f, 0.0f }; // 効果の色
};
