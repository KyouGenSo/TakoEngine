#pragma once
#include "Quaternion.h"
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Tako {

  /// <summary>
  /// クォータニオン演算ユーティリティ名前空間
  /// 3D 回転をジンバルロックなしで表現するクォータニオンの全操作を提供
  /// 球面線形補間（Slerp）による滑らかな回転アニメーションをサポート
  /// </summary>
  namespace Quat {
    /// <summary>
    /// 単位クォータニオン（回転なし）を生成
    /// </summary>
    Quaternion Identity();

    /// <summary>
    /// クォータニオンの加算
    /// </summary>
    Quaternion Add(const Quaternion& q1, const Quaternion& q2);

    /// <summary>
    /// クォータニオンの減算
    /// </summary>
    Quaternion Subtract(const Quaternion& q1, const Quaternion& q2);

    /// <summary>
    /// クォータニオンのスカラー倍
    /// </summary>
    Quaternion Multiply(const Quaternion& q, float scaler);

    /// <summary>
    /// クォータニオンの乗算（回転の合成）
    /// </summary>
    Quaternion Multiply(const Quaternion& q1, const Quaternion& q2);

    /// <summary>
    /// クォータニオンのノルム（大きさ）を計算
    /// </summary>
    float Norm(const Quaternion& q);

    /// <summary>
    /// クォータニオンを正規化
    /// </summary>
    Quaternion Normalize(const Quaternion& q);

    /// <summary>
    /// 共役クォータニオンを計算
    /// </summary>
    Quaternion Conjugate(const Quaternion& q);

    /// <summary>
    /// 逆クォータニオンを計算
    /// </summary>
    Quaternion Inverse(const Quaternion& q);

    /// <summary>
    /// 球面線形補間（Spherical Linear Interpolation）
    /// </summary>
    Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t);

    /// <summary>
    /// 任意軸周りの回転クォータニオンを生成
    /// </summary>
    Quaternion MakeRotateAxisAngle(const Vector3& axis, float angle);

    /// <summary>
    /// クォータニオンをベクトル（虚部）に変換
    /// </summary>
    Vector3 ToVec3(const Quaternion& q);

    /// <summary>
    /// クォータニオンを回転行列に変換
    /// </summary>
    Matrix4x4 ToMatrix(const Quaternion& q);

    /// <summary>
    /// ベクトルをクォータニオンで回転
    /// </summary>
    Vector3 RotateVec3(const Vector3& v, const Quaternion& q);
  }

} // namespace Tako
