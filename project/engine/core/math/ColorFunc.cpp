#include "ColorFunc.h"

#include <cmath>

namespace Tako
{
  namespace Color {
    Vector4 FromHex(uint32_t rgb, float alpha) {
      return Vector4(
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha);
    }

    Vector4 WithAlpha(const Vector4& color, float alpha) {
      return Vector4(color.x, color.y, color.z, alpha);
    }

    Vector4 FromHSV(float hue, float saturation, float value, float alpha) {
      // 色相環を 60 度ずつ 6 区間に分け、区間ごとに RGB のどれが最大・中間になるかが入れ替わる
      const float h = (hue - std::floor(hue)) * 6.0f;
      const float c = value * saturation;
      const float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
      const float m = value - c;

      float r = 0.0f, g = 0.0f, b = 0.0f;
      switch (static_cast<int>(h)) {
      case 0:  r = c; g = x; break;
      case 1:  r = x; g = c; break;
      case 2:  g = c; b = x; break;
      case 3:  g = x; b = c; break;
      case 4:  r = x; b = c; break;
      default: r = c; b = x; break;
      }
      return Vector4(r + m, g + m, b + m, alpha);
    }
  }
}
