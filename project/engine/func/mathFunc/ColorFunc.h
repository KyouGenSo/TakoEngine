#pragma once
#include "Vector4.h"

#include <cstdint>

namespace Tako {

  /// <summary>
  /// 色（RGBA を格納した Vector4）ユーティリティ名前空間
  /// 16 進数や HSV からの生成、アルファの差し替えなど色の組み立てによく使う処理を提供
  /// </summary>
  namespace Color {
    inline constexpr Vector4 kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
    inline constexpr Vector4 kClear = { 0.0f, 0.0f, 0.0f, 0.0f };  ///< 透明（RGBA すべて 0）

    /// <summary>
    /// 0xRRGGBB 形式の 16 進数から色を作る
    /// </summary>
    Vector4 FromHex(uint32_t rgb, float alpha = 1.0f);

    /// <summary>
    /// アルファだけを差し替えた色を返す
    /// </summary>
    Vector4 WithAlpha(const Vector4& color, float alpha);

    /// <summary>
    /// HSV から色を作る。hue は 0〜1 で 1 周し範囲外は巡回する。saturation / value は 0〜1
    /// </summary>
    Vector4 FromHSV(float hue, float saturation, float value, float alpha = 1.0f);
  }

} // namespace Tako
