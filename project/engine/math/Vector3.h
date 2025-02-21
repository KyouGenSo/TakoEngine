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
    Vector3 result;
		float length = std::sqrt(x * x + y * y + z * z);

		// ゼロベクトルの場合はそのまま返す
		if (length <= std::numeric_limits<float>::epsilon()) {
      result.x = 0.0f;
      result.y = 0.0f;
      result.z = 0.0f;
			return result;  // そのままゼロベクトルを返す
		}

		float invLength = 1.0f / length; // 除算回数を減らして最適化
    result = {x * invLength, y * invLength, z * invLength };
		return result;
	}

	float Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}
};
