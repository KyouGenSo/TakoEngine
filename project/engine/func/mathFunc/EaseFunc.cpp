#include "EaseFunc.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Tako {
  namespace Ease {

    // ============================================================
    // Linear
    // ============================================================
    float Linear(float t) {
      return t;
    }

    // ============================================================
    // Quad（2次）
    // ============================================================
    float InQuad(float t) {
      return t * t;
    }

    float OutQuad(float t) {
      float u = 1.0f - t;
      return 1.0f - u * u;
    }

    float InOutQuad(float t) {
      if (t < 0.5f) {
        return 2.0f * t * t;
      }
      float u = -2.0f * t + 2.0f;
      return 1.0f - (u * u) / 2.0f;
    }

    // ============================================================
    // Cubic（3次）
    // ============================================================
    float InCubic(float t) {
      return t * t * t;
    }

    float OutCubic(float t) {
      float u = 1.0f - t;
      return 1.0f - u * u * u;
    }

    float InOutCubic(float t) {
      if (t < 0.5f) {
        return 4.0f * t * t * t;
      }
      float u = -2.0f * t + 2.0f;
      return 1.0f - (u * u * u) / 2.0f;
    }

    // ============================================================
    // Quart（4次）
    // ============================================================
    float InQuart(float t) {
      return t * t * t * t;
    }

    float OutQuart(float t) {
      float u = 1.0f - t;
      return 1.0f - u * u * u * u;
    }

    float InOutQuart(float t) {
      if (t < 0.5f) {
        return 8.0f * t * t * t * t;
      }
      float u = -2.0f * t + 2.0f;
      return 1.0f - (u * u * u * u) / 2.0f;
    }

    // ============================================================
    // Quint（5次）
    // ============================================================
    float InQuint(float t) {
      return t * t * t * t * t;
    }

    float OutQuint(float t) {
      float u = 1.0f - t;
      return 1.0f - u * u * u * u * u;
    }

    float InOutQuint(float t) {
      if (t < 0.5f) {
        return 16.0f * t * t * t * t * t;
      }
      float u = -2.0f * t + 2.0f;
      return 1.0f - (u * u * u * u * u) / 2.0f;
    }

    // ============================================================
    // Sine（正弦）
    // ============================================================
    float InSine(float t) {
      const float pi = std::numbers::pi_v<float>;
      return 1.0f - std::cos((t * pi) / 2.0f);
    }

    float OutSine(float t) {
      const float pi = std::numbers::pi_v<float>;
      return std::sin((t * pi) / 2.0f);
    }

    float InOutSine(float t) {
      const float pi = std::numbers::pi_v<float>;
      return -(std::cos(pi * t) - 1.0f) / 2.0f;
    }

    // ============================================================
    // Expo（指数）
    // ============================================================
    float InExpo(float t) {
      return (t == 0.0f) ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
    }

    float OutExpo(float t) {
      return (t == 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    }

    float InOutExpo(float t) {
      if (t == 0.0f) {
        return 0.0f;
      }
      if (t == 1.0f) {
        return 1.0f;
      }
      if (t < 0.5f) {
        return std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f;
      }
      return (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
    }

    // ============================================================
    // Circ（円弧）
    // ============================================================
    float InCirc(float t) {
      return 1.0f - std::sqrt(1.0f - t * t);
    }

    float OutCirc(float t) {
      float u = t - 1.0f;
      return std::sqrt(1.0f - u * u);
    }

    float InOutCirc(float t) {
      if (t < 0.5f) {
        float u = 2.0f * t;
        return (1.0f - std::sqrt(1.0f - u * u)) / 2.0f;
      }
      float u = -2.0f * t + 2.0f;
      return (std::sqrt(1.0f - u * u) + 1.0f) / 2.0f;
    }

    // ============================================================
    // Back（行き過ぎて戻る）
    // ============================================================
    float InBack(float t) {
      const float c1 = 1.70158f;
      const float c3 = c1 + 1.0f;
      return c3 * t * t * t - c1 * t * t;
    }

    float OutBack(float t) {
      const float c1 = 1.70158f;
      const float c3 = c1 + 1.0f;
      float u = t - 1.0f;
      return 1.0f + c3 * u * u * u + c1 * u * u;
    }

    float InOutBack(float t) {
      const float c1 = 1.70158f;
      const float c2 = c1 * 1.525f;
      if (t < 0.5f) {
        float u = 2.0f * t;
        return (u * u * ((c2 + 1.0f) * u - c2)) / 2.0f;
      }
      float u = 2.0f * t - 2.0f;
      return (u * u * ((c2 + 1.0f) * u + c2) + 2.0f) / 2.0f;
    }

    // ============================================================
    // Elastic（弾性・減衰振動）
    // ============================================================
    float InElastic(float t) {
      if (t == 0.0f) {
        return 0.0f;
      }
      if (t == 1.0f) {
        return 1.0f;
      }
      const float c4 = (2.0f * std::numbers::pi_v<float>) / 3.0f;
      return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((10.0f * t - 10.75f) * c4);
    }

    float OutElastic(float t) {
      if (t == 0.0f) {
        return 0.0f;
      }
      if (t == 1.0f) {
        return 1.0f;
      }
      const float c4 = (2.0f * std::numbers::pi_v<float>) / 3.0f;
      return std::pow(2.0f, -10.0f * t) * std::sin((10.0f * t - 0.75f) * c4) + 1.0f;
    }

    float InOutElastic(float t) {
      if (t == 0.0f) {
        return 0.0f;
      }
      if (t == 1.0f) {
        return 1.0f;
      }
      const float c5 = (2.0f * std::numbers::pi_v<float>) / 4.5f;
      if (t < 0.5f) {
        return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f;
      }
      return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
    }

    // ============================================================
    // Bounce（跳ね返り）
    // ============================================================
    float OutBounce(float t) {
      const float n1 = 7.5625f;
      const float d1 = 2.75f;

      if (t < 1.0f / d1) {
        return n1 * t * t;
      }
      else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
      }
      else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
      }
      else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
      }
    }

    float InBounce(float t) {
      return 1.0f - OutBounce(1.0f - t);
    }

    float InOutBounce(float t) {
      if (t < 0.5f) {
        return (1.0f - OutBounce(1.0f - 2.0f * t)) / 2.0f;
      }
      return (1.0f + OutBounce(2.0f * t - 1.0f)) / 2.0f;
    }

    // ============================================================
    // 多項式平滑（Hermite 系）
    // ============================================================
    float SmoothStep(float t) {
      return t * t * (3.0f - 2.0f * t);
    }

    float SmootherStep(float t) {
      return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // ============================================================
    // CubicBezier（カスタム制御点）
    // ============================================================
    float CubicBezier(float t, float x1, float y1, float x2, float y2) {
      constexpr float kEpsilon = 1e-6f;

      if (t <= 0.0f) {
        return 0.0f;
      }
      if (t >= 1.0f) {
        return 1.0f;
      }

      // x1/x2 が [0,1] を外れると曲線が横に折り返し、同じ時間 t に対応する点が複数できてしまうため clamp
      x1 = std::clamp(x1, 0.0f, 1.0f);
      x2 = std::clamp(x2, 0.0f, 1.0f);

      // ベジェ曲線を計算しやすい多項式の形 ((a*u + b)*u + c)*u に展開した係数
      float cx = 3.0f * x1;
      float bx = 3.0f * (x2 - x1) - cx;
      float ax = 1.0f - cx - bx;

      auto sampleX = [=](float u) { return ((ax * u + bx) * u + cx) * u; };

      // ベジェ曲線は媒介変数 u で進むため、横軸（時間）が t になる u をまず探す。
      // 高速な Newton 法を試し、収束しなければ確実な二分法に切り替える
      float u = t;
      for (int i = 0; i < 8; ++i) {
        float diff = sampleX(u) - t;
        if (std::abs(diff) < kEpsilon) {
          break;
        }
        float derivative = (3.0f * ax * u + 2.0f * bx) * u + cx;
        if (std::abs(derivative) < kEpsilon) {
          break;
        }
        u -= diff / derivative;
      }
      u = std::clamp(u, 0.0f, 1.0f);

      if (std::abs(sampleX(u) - t) >= kEpsilon) {
        float lo = 0.0f;
        float hi = 1.0f;
        while (hi - lo > kEpsilon) {
          u = (lo + hi) * 0.5f;
          if (sampleX(u) < t) {
            lo = u;
          }
          else {
            hi = u;
          }
        }
      }

      // 求めた u における縦軸（進行度）を返す
      float cy = 3.0f * y1;
      float by = 3.0f * (y2 - y1) - cy;
      float ay = 1.0f - cy - by;
      return ((ay * u + by) * u + cy) * u;
    }

  } // namespace Ease
} // namespace Tako
