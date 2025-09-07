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

	Vector3& operator+=(const Vector3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	Vector3& operator-=(const Vector3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	Vector3& operator*=(float s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}

	Vector3& operator/=(float s) {
		x /= s;
		y /= s;
		z /= s;
		return *this;
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

	friend Vector3 operator*(float s, const Vector3& v) {
		return v * s;
	}

	Vector3 operator/(float s) const {
		Vector3 result;
		result.x = x / s;
		result.y = y / s;
		result.z = z / s;

		return result;
	}

	Vector3 operator-() const {
		return {-x, -y, -z};
	}

	bool operator==(const Vector3& v) const {
		return (std::abs(x - v.x) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(y - v.y) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(z - v.z) <= std::numeric_limits<float>::epsilon());
	}

	bool operator!=(const Vector3& v) const {
		return !(*this == v);
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

	float LengthSquared() const {
		return x * x + y * y + z * z;
	}

	float Dot(const Vector3& v) const {
		return x * v.x + y * v.y + z * v.z;
	}

	Vector3 Cross(const Vector3& v) const {
		return {
			y * v.z - z * v.y,
			z * v.x - x * v.z,
			x * v.y - y * v.x
		};
	}

	float Distance(const Vector3& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		float dz = z - v.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	float DistanceSquared(const Vector3& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		float dz = z - v.z;
		return dx * dx + dy * dy + dz * dz;
	}

	static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
		return {
			a.x + t * (b.x - a.x),
			a.y + t * (b.y - a.y),
			a.z + t * (b.z - a.z)
		};
	}

	Vector3 Reflect(const Vector3& normal) const {
		float dot = this->Dot(normal);
		return *this - normal * (2.0f * dot);
	}

	Vector3 Project(const Vector3& onto) const {
		float lengthSquared = onto.LengthSquared();
		if (lengthSquared <= std::numeric_limits<float>::epsilon()) {
			return {0.0f, 0.0f, 0.0f};
		}
		float scalar = this->Dot(onto) / lengthSquared;
		return onto * scalar;
	}
};
