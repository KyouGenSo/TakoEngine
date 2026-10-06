#pragma once
#include "IPostEffect.h"

namespace Tako {

  /// <summary>
  /// 輝度の段差からエッジを推定してぼかす画面空間アンチエイリアス（PostEffectManager がチェーン末尾に自動で掛ける）
  /// </summary>
  class FXAA : public IPostEffect
  {
  public: //メンバー関数
    void Apply(
      uint32_t                    inputSrvIndex,
      D3D12_CPU_DESCRIPTOR_HANDLE outputRtvHandle,
      uint32_t                    depthSrvIndex,
      const Vector4& clearColor
    ) override;

    void DrawImgui() override {}

  private: //非公開関数
    void CreateRootSignature() override;
    void CreatePSO() override;
  };

} // namespace Tako
