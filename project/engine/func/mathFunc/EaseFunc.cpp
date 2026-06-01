#include "EaseFunc.h"

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

  } // namespace Ease
} // namespace Tako
