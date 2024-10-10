#pragma once
#include "Vector3.h"

namespace Vec3 {
	Vector3 Add(const Vector3& v1, const Vector3& v2);

	Vector3 Subtract(const Vector3& v1, const Vector3& v2);

	Vector3 Multiply(const Vector3& v, float scaler);

	float Dot(const Vector3& v1, const Vector3& v2);

	double Length(const Vector3& v);

	Vector3 Normalize(const Vector3& v);

	Vector3 Cross(const Vector3& v1, const Vector3& v2);
}

