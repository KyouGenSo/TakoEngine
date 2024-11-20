#pragma once
#include "Vec3Func.h"

struct Quaternion {
	float x;
	float y;
	float z;
	float w;

	Quaternion operator*=(const Quaternion& q) {
		Quaternion result;
		result.x = w * q.x + x * q.w + y * q.z - z * q.y;
		result.y = w * q.y + y * q.w + z * q.x - x * q.z;
		result.z = w * q.z + z * q.w + x * q.y - y * q.x;
		result.w = w * q.w - x * q.x - y * q.y - z * q.z;

		return result;
	}

	Quaternion operator*(const Quaternion& q) const {
		Quaternion result;
		result.x = w * q.x + x * q.w + y * q.z - z * q.y;
		result.y = w * q.y + y * q.w + z * q.x - x * q.z;
		result.z = w * q.z + z * q.w + x * q.y - y * q.x;
		result.w = w * q.w - x * q.x - y * q.y - z * q.z;

		return result;
	}
};