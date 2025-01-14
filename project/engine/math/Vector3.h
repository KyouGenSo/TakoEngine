#pragma once
#include <limits>
#include <cmath>

/// <summary>
/// 3次元ベクトル
/// </summary>
struct Vector3 final {
	float x;
	float y;
	float z;

	Vector3 operator+=(const Vector3& v) {
		Vector3 result;
		x += v.x;
		y += v.y;
		z += v.z;

		return result;
	}

	Vector3 operator-=(const Vector3& v) {
		Vector3 result;
		x -= v.x;
		y -= v.y;
		z -= v.z;

		return result;
	}

	Vector3 operator*=(float s) {
		Vector3 result;
		x *= s;
		y *= s;
		z *= s;

		return result;
	}

	Vector3 operator/=(float s) {
		Vector3 result;
		x /= s;
		y /= s;
		z /= s;

		return result;
	}

	Vector3 operator+(const Vector3& v) const {
		Vector3 result;
		result.x = x + v.x;
		result.y = y + v.y;
		result.z = z + v.z;

		return result;
	}

	Vector3 operator-(const Vector3& v) const {
		Vector3 result;
		result.x = x - v.x;
		result.y = y - v.y;
		result.z = z - v.z;

		return result;
	}

	Vector3 operator*(float s) const {
		Vector3 result;
		result.x = x * s;
		result.y = y * s;
		result.z = z * s;

		return result;
	}

	Vector3 operator/(float s) const {
		Vector3 result;
		result.x = x / s;
		result.y = y / s;
		result.z = z / s;

		return result;
	}

	Vector3 Normalize() const {
		float length = std::sqrt(x * x + y * y + z * z);

		// ゼロベクトルの場合はそのまま返す
		if (length <= std::numeric_limits<float>::epsilon()) {
			return Vector3(0.0f, 0.0f, 0.0f);  // そのままゼロベクトルを返す
		}

		float invLength = 1.0f / length; // 除算回数を減らして最適化
		return Vector3(x * invLength, y * invLength, z * invLength);
	}

	float Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}
};
