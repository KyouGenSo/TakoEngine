#pragma once
#include "Vector3.h"

namespace Tako {

  /// <summary>
  /// 3次元ベクトル演算ユーティリティ名前空間
  /// 加算・減算・内積・外積など基本的なベクトル演算から
  /// 線形補間（Lerp）、球面線形補間（Slerp）などのアニメーション用補間機能を提供
  /// </summary>
  namespace Vec3 {
    /// <summary>ベクトルの加算</summary>
    Vector3 Add(const Vector3& v1, const Vector3& v2);

    /// <summary>ベクトルの減算</summary>
    Vector3 Subtract(const Vector3& v1, const Vector3& v2);

    /// <summary>ベクトルのスカラー倍</summary>
    Vector3 Multiply(const Vector3& v, float scaler);

    /// <summary>内積を計算</summary>
    float Dot(const Vector3& v1, const Vector3& v2);

    /// <summary>ベクトルの長さを計算</summary>
    double Length(const Vector3& v);

    /// <summary>ベクトルを正規化（長さを1にする）</summary>
    Vector3 Normalize(const Vector3& v);

    /// <summary>外積を計算（2つのベクトルに垂直なベクトルを生成）</summary>
    Vector3 Cross(const Vector3& v1, const Vector3& v2);

    /// <summary>浮動小数点数の線形補間</summary>
    float Lerp(float a, float b, float t);

    /// <summary>ベクトルの線形補間</summary>
    Vector3 Lerp(const Vector3& a, const Vector3& b, float t);

    /// <summary>ベクトルの球面線形補間</summary>
    Vector3 Slerp(const Vector3& v1, const Vector3& v2, float t);

    /// <summary>角度の最短経路補間（±180度を考慮）</summary>
    float LerpShortAngle(float thetaA, float thetaB, float t);
  }

} // namespace Tako

