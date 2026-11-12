#include "Vec3Func.h"
#include<math.h>

#include <numbers>

namespace Tako
{
  namespace Vec3 {
    Vector3 Add(const Vector3& v1, const Vector3& v2) {
      return Vector3(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
    }

    Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
      return Vector3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
    }

    Vector3 Multiply(const Vector3& v, float scaler) {
      return Vector3(v.x * scaler, v.y * scaler, v.z * scaler);
    }

    float Dot(const Vector3& v1, const Vector3& v2) {
      return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    double Length(const Vector3& v) {
      return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    Vector3 Normalize(const Vector3& v) {
      double length = Length(v);
      return Vector3(v.x / static_cast<float>(length), v.y / static_cast<float>(length), v.z / static_cast<float>(length));
    }

    Vector3 Cross(const Vector3& v1, const Vector3& v2) {
      return Vector3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x);
    }

    float Lerp(float a, float b, float t)
    {
      return a + (b - a) * t;
    }

    Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
    {
      return Vector3(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t));
    }

    Vector3 Slerp(const Vector3& v1, const Vector3& v2, float t)
    {
      float dot = Dot(v1, v2);

      dot = dot > 1.0f ? 1.0f : dot;
      dot = dot < -1.0f ? -1.0f : dot;

      float theta = (float)acos(dot) * t;

      float sinTheta = (float)sin(theta);

      float sinThetaFrom = (float)sin((1.0f - t) * theta);
      float sinThetaTo = (float)sin(t * theta);

      float length1 = static_cast<float>(Length(v1));
      float length2 = static_cast<float>(Length(v2));

      float length = Lerp(length1, length2, t);

      if (sinTheta < 1.0e-5) {

        return v1;

      }
      else {

        return Multiply(Add(Multiply(v1, sinThetaFrom / sinTheta), Multiply(v2, sinThetaTo / sinTheta)), length);
      }
    }

    float LerpShortAngle(float thetaA, float thetaB, float t)
    {
      float diff = thetaB - thetaA;
      float pi = std::numbers::pi_v<float>;

      // 2πから-2πに補正
      if (diff > pi * 2) {
        diff = std::fmod(diff, pi * 2);
      }
      else if (diff < -pi * 2) {
        diff = std::fmod(diff, pi * 2);
      }

      // 180度以上の差がある場合は、逆回転する方向に補間する
      if (diff > pi) {
        diff -= pi * 2;
      }
      else if (diff < -pi) {
        diff += pi * 2;
      }

      return thetaA + diff * t;
    }

    Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
    {
      float t2 = t * t;
      float t3 = t2 * t;

      // 4点の混合比率。この重みで足し合わせると曲線上の点になる（t=0 で p1 の重みが 1、t=1 で p2 の重みが 1）
      float w0 = 0.5f * (-t3 + 2.0f * t2 - t);
      float w1 = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
      float w2 = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
      float w3 = 0.5f * (t3 - t2);

      return Vector3(
        w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x,
        w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y,
        w0 * p0.z + w1 * p1.z + w2 * p2.z + w3 * p3.z);
    }
  }
} // namespace Tako

