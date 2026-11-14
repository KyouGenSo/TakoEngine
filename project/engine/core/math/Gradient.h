#pragma once
#include "ColorFunc.h"
#include "Reflection.h"

#include <vector>

namespace Tako {

  /// <summary>
  /// 0〜1 の位置ごとの色。段階の間は線形に補間する。TPROPERTY にすると Inspector で段階を編集できる
  /// </summary>
  TSTRUCT()
  struct TAKO_API Gradient {
    GENERATED_STRUCT_BODY()

  public: //構造体
    /// <summary>
    /// 段階 1 つ
    /// </summary>
    TSTRUCT()
    struct TAKO_API Key {
      GENERATED_STRUCT_BODY()

      TPROPERTY(Edit, ClampMin = 0, ClampMax = 1, Speed = 0.01)
      float time = 0.0f;
      TPROPERTY(Edit)
      Color color = Color::kWhite;
    };

  public: //メンバー関数
    /// <summary>
    /// t の位置の色。段階の順は問わない。範囲の外は端の色、段階が無ければ白
    /// </summary>
    Color Evaluate(float t) const
    {
      const Key* lower = nullptr;
      const Key* upper = nullptr;
      for (const Key& key : keys) {
        if (key.time <= t && (!lower || key.time >= lower->time)) { lower = &key; }
        if (key.time >= t && (!upper || key.time < upper->time)) { upper = &key; }
      }
      if (!lower && !upper) { return Color::kWhite; }
      if (!lower) { return upper->color; }
      if (!upper || upper->time <= lower->time) { return lower->color; }
      return Color::Lerp(lower->color, upper->color, (t - lower->time) / (upper->time - lower->time));
    }

  public: //メンバー変数
    TPROPERTY(Edit)
    std::vector<Key> keys;
  };

} // namespace Tako
