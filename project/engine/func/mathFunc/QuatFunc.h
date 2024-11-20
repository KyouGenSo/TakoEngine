#pragma once
#include "Quaternion.h"
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Quat {
	Quaternion Add(const Quaternion& q1, const Quaternion& q2);

	Quaternion Subtract(const Quaternion& q1, const Quaternion& q2);

	Quaternion Multiply(const Quaternion& q, float scaler);

	Quaternion Multiply(const Quaternion& q1, const Quaternion& q2);

	float Norm(const Quaternion& q);

	Quaternion Normalize(const Quaternion& q);

	Quaternion Conjugate(const Quaternion& q);

	Quaternion Inverse(const Quaternion& q);

	Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t);

	Vector3 ToVec3(const Quaternion& q);

	Matrix4x4 ToMatrix(const Quaternion& q);
}
