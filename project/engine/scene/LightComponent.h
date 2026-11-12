#pragma once
#include "Component.h"
#include "Vector4.h"

#include <numbers>

namespace Tako {

  /// <summary>
  /// Actor の位置から、Actor の +Z の向きへ照らすライト。毎フレーム Draw で登録する（平行光源は 1 つだけなので、後に描いたものが勝つ）
  /// </summary>
  TCLASS(DisplayName = "Light")
  class LightComponent : public Component {
    GENERATED_BODY()

  public: //構造体
    TENUM()
    enum class Type { Directional, Point, Spot };

  public: //メンバー関数
    LightComponent();

    void Draw() override;

    /// <summary>
    /// 平行光源は Draw を止めても最後の値が残るので、オフになったら強さを 0 にする
    /// </summary>
    void OnActiveChanged(bool active) override;

  private: //メンバー変数
    TPROPERTY(Edit)
    Type    type_           = Type::Directional;
    TPROPERTY(Edit, Color)
    Vector4 color_          = { 1.0f, 1.0f, 1.0f, 1.0f };
    TPROPERTY(Edit, ClampMin = 0, Speed = 0.01)
    float   intensity_      = 1.0f;
    TPROPERTY(Edit, Tooltip = "点光源・スポットライトの届く距離", ClampMin = 0, Speed = 0.1)
    float   range_          = 10.0f;
    TPROPERTY(Edit, Tooltip = "点光源・スポットライトの減衰の強さ", ClampMin = 0, Speed = 0.01)
    float   decay_          = 1.0f;
    TPROPERTY(Edit, Angle, Tooltip = "スポットライトのコーンの半角", ClampMin = 1, ClampMax = 89)
    float   spotAngle_      = std::numbers::pi_v<float> / 3.0f;
    TPROPERTY(Edit, Tooltip = "平行光源の影を落とす、カメラからの距離", ClampMin = 1, Speed = 1)
    float   shadowDistance_ = 0.0f;
  };

} // namespace Tako
