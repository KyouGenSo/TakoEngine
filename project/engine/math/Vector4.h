#pragma once
#include <limits>
#include <cmath>

/// <summary>
/// 4次元ベクトル
/// </summary>
struct Vector4 final {
	float x;
	float y;
	float z;
	float w;

	Vector4& operator+=(const Vector4& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
		return *this;
	}

	Vector4& operator-=(const Vector4& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		w -= v.w;
		return *this;
	}

	Vector4& operator*=(float s) {
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}

	Vector4& operator/=(float s) {
		x /= s;
		y /= s;
		z /= s;
		w /= s;
		return *this;
	}

	Vector4 operator+(const Vector4& v) const {
		Vector4 result;
		result.x = x + v.x;
		result.y = y + v.y;
		result.z = z + v.z;
		result.w = w + v.w;

		return result;
	}

	Vector4 operator-(const Vector4& v) const {
		Vector4 result;
		result.x = x - v.x;
		result.y = y - v.y;
		result.z = z - v.z;
		result.w = w - v.w;

		return result;
	}

	Vector4 operator*(float s) const {
		Vector4 result;
		result.x = x * s;
		result.y = y * s;
		result.z = z * s;
		result.w = w * s;

		return result;
	}

	friend Vector4 operator*(float s, const Vector4& v) {
		return v * s;
	}

	Vector4 operator/(float s) const {
		Vector4 result;
		result.x = x / s;
		result.y = y / s;
		result.z = z / s;
		result.w = w / s;

		return result;
	}

	Vector4 operator-() const {
		return {-x, -y, -z, -w};
	}

	bool operator==(const Vector4& v) const {
		return (std::abs(x - v.x) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(y - v.y) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(z - v.z) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(w - v.w) <= std::numeric_limits<float>::epsilon());
	}

	bool operator!=(const Vector4& v) const {
		return !(*this == v);
	}

	Vector4 Normalize() const {
		Vector4 result;
		float length = std::sqrt(x * x + y * y + z * z + w * w);

		// ゼロベクトルの場合はそのまま返す
		if (length <= std::numeric_limits<float>::epsilon()) {
			result.x = 0.0f;
			result.y = 0.0f;
			result.z = 0.0f;
			result.w = 0.0f;
			return result;  // そのままゼロベクトルを返す
		}

		float invLength = 1.0f / length; // 除算回数を減らして最適化
		result = {x * invLength, y * invLength, z * invLength, w * invLength};
		return result;
	}

	float Length() const {
		return std::sqrt(x * x + y * y + z * z + w * w);
	}

	float LengthSquared() const {
		return x * x + y * y + z * z + w * w;
	}

	float Dot(const Vector4& v) const {
		return x * v.x + y * v.y + z * v.z + w * v.w;
	}

	float Distance(const Vector4& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		float dz = z - v.z;
		float dw = w - v.w;
		return std::sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
	}

	float DistanceSquared(const Vector4& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		float dz = z - v.z;
		float dw = w - v.w;
		return dx * dx + dy * dy + dz * dz + dw * dw;
	}

	static Vector4 Lerp(const Vector4& a, const Vector4& b, float t) {
		return {
			a.x + t * (b.x - a.x),
			a.y + t * (b.y - a.y),
			a.z + t * (b.z - a.z),
			a.w + t * (b.w - a.w)
		};
	}
};
