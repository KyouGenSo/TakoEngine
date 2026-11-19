#pragma once
#include <array>
#include <cmath>
#include <cstdint>

#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

namespace Tako {

  /// <summary>
  /// 視点の範囲（視錐台・正射影の箱）を囲む面。球がこの範囲に掛かるかの判定に使う
  /// </summary>
  struct Frustum {
    std::array<Vector4, 6> planes{};        ///< 範囲の境の面（内側が正、法線は正規化済み）。左・右・下・上・奥・手前の順
    uint32_t               planeCount = 0;  ///< 判定に使う面の数。0 なら何も捨てない

    /// <summary>
    /// viewProjection（行ベクトルに右から掛ける行列。深度は 0〜1）の範囲の面を作る
    /// </summary>
    /// <param name="useNearPlane">false なら手前の面では捨てない</param>
    static Frustum FromViewProjection(const Matrix4x4& viewProjection, bool useNearPlane = true)
    {
      // クリップ空間の条件（-w <= x <= w、-w <= y <= w、0 <= z <= w）は、行列の列の足し引きでワールドの面になる
      const auto plane = [&](int column, float sign, bool addW) {
        Vector4 result{};
        float*  elements[] = { &result.x, &result.y, &result.z, &result.w };
        for (int row = 0; row < 4; ++row) {
          *elements[row] = viewProjection.m[row][column] * sign + (addW ? viewProjection.m[row][3] : 0.0f);
        }
        const float length = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z);
        return length > 0.0f ? result / length : result;
      };

      Frustum frustum;
      frustum.planes     = { plane(0, 1.0f, true), plane(0, -1.0f, true), plane(1, 1.0f, true), plane(1, -1.0f, true), plane(2, -1.0f, true), plane(2, 1.0f, false) };
      frustum.planeCount = useNearPlane ? 6 : 5;
      return frustum;
    }

    /// <summary>
    /// 球が範囲に掛かるか（radius が 0 以下なら、範囲が分からない物として常に掛かる）
    /// </summary>
    bool Intersects(const Vector3& center, float radius) const
    {
      if (radius <= 0.0f) {
        return true;
      }
      for (uint32_t i = 0; i < planeCount; ++i) {
        const Vector4& plane = planes[i];
        if (plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w < -radius) {
          return false;
        }
      }
      return true;
    }

    bool operator==(const Frustum&) const = default;
  };

} // namespace Tako
