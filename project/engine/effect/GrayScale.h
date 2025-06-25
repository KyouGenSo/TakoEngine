#pragma once
#include "IPostEffect.h"

class GrayScale : public IPostEffect
{
public:

  // 初期化
  void Initialize(DX12Basic* dx12, std::string shaderName) override;

  // 描画
  void Draw() override;

  // 描画設定
  void SetDrawSetting() override;

  // Debug描画
  void DrawImgui() override;

private:

  void CreateRootSignature() override;
  void CreatePSO() override;
  void CreateCBV() override;
};

