#pragma once
#include "Camera.h"
#include "Component.h"

#include <cstdint>

namespace Tako {

  /// <summary>
  /// Actor の位置と向きから見るカメラ（UE の UCameraComponent）。シーンの中で priority が一番高いものが、Edit 中も Play 中も毎フレーム Renderer のカメラへ写す
  /// </summary>
  TCLASS()
  class CameraComponent : public Component {
    GENERATED_BODY()

  public: //メンバー関数
    void Draw() override;

    //=====================================
    //Setter
    //=====================================
    void SetFovY(float fovY) { fovY_ = fovY; }

  private: //メンバー変数
    TPROPERTY(Edit, Angle, DisplayName = "Field Of View", Tooltip = "縦の視野角", ClampMin = 1, ClampMax = 179)
    float   fovY_     = Camera::kDefaultFovY;
    TPROPERTY(Edit, ClampMin = 0.001, Speed = 0.01)
    float   nearClip_ = Camera::kDefaultNearClip;
    TPROPERTY(Edit, ClampMin = 0.01, Speed = 1)
    float   farClip_  = Camera::kDefaultFarClip;
    TPROPERTY(Edit, Tooltip = "シーンに複数あるときは大きいものを使う（同じなら後に描いたもの）")
    int32_t priority_ = 0;
  };

} // namespace Tako
