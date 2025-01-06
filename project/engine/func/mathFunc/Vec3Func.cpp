#include "Vec3Func.h"
#include<math.h>

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
		return Vector3(v.x / (float)length, v.y / (float)length, v.z / (float)length);
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

		float length1 = (float)Length(v1);
		float length2 = (float)Length(v2);

		float length = Lerp(length1, length2, t);

		if (sinTheta < 1.0e-5) {

			return v1;

		} else {

			return Multiply(Add(Multiply(v1, sinThetaFrom / sinTheta), Multiply(v2, sinThetaTo / sinTheta)), length);
		}
	}

	float LerpShortAngle(float thetaA, float thetaB, float t)
	{
		float diff = thetaB - thetaA;
		float pi = 3.14159265358979323846f;

		// 2πから-2πに補正
		if (diff > pi * 2) {
			diff = std::fmod(diff, pi * 2);
		} else if (diff < -pi * 2) {
			diff = std::fmod(diff, pi * 2);
		}

		// 180度以上の差がある場合は、逆回転する方向に補間する
		if (diff > pi) {
			diff -= pi * 2;
		} else if (diff < -pi) {
			diff += pi * 2;
		}

		return thetaA + diff * t;
	}
	float Rand(float min, float max)
	{
		return min + (float)rand() / ((float)RAND_MAX / (max - min));
	}
}

