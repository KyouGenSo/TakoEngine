#pragma once
#include <limits>
#include <cmath>

/// <summary>
/// 2次元ベクトル
/// </summary>
struct Vector2 final {
	float x;
	float y;

	Vector2& operator+=(const Vector2& v) {
		x += v.x;
		y += v.y;
		return *this;
	}

	Vector2& operator-=(const Vector2& v) {
		x -= v.x;
		y -= v.y;
		return *this;
	}

	Vector2& operator*=(float s) {
		x *= s;
		y *= s;
		return *this;
	}

	Vector2& operator/=(float s) {
		x /= s;
		y /= s;
		return *this;
	}

	Vector2 operator+(const Vector2& v) const {
		Vector2 result;
		result.x = x + v.x;
		result.y = y + v.y;

		return result;
	}

	Vector2 operator-(const Vector2& v) const {
		Vector2 result;
		result.x = x - v.x;
		result.y = y - v.y;

		return result;
	}

	Vector2 operator*(float s) const {
		Vector2 result;
		result.x = x * s;
		result.y = y * s;

		return result;
	}

	friend Vector2 operator*(float s, const Vector2& v) {
		return v * s;
	}

	Vector2 operator/(float s) const {
		Vector2 result;
		result.x = x / s;
		result.y = y / s;

		return result;
	}

	Vector2 operator-() const {
		return {-x, -y};
	}

	bool operator==(const Vector2& v) const {
		return (std::abs(x - v.x) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(y - v.y) <= std::numeric_limits<float>::epsilon());
	}

	bool operator!=(const Vector2& v) const {
		return !(*this == v);
	}

	Vector2 Normalize() const {
		Vector2 result;
		float length = std::sqrt(x * x + y * y);

		// ゼロベクトルの場合はそのまま返す
		if (length <= std::numeric_limits<float>::epsilon()) {
			result.x = 0.0f;
			result.y = 0.0f;
			return result;  // そのままゼロベクトルを返す
		}

		float invLength = 1.0f / length; // 除算回数を減らして最適化
		result = {x * invLength, y * invLength};
		return result;
	}

	float Length() const {
		return std::sqrt(x * x + y * y);
	}

	float LengthSquared() const {
		return x * x + y * y;
	}

	float Dot(const Vector2& v) const {
		return x * v.x + y * v.y;
	}

	float Distance(const Vector2& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	float DistanceSquared(const Vector2& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		return dx * dx + dy * dy;
	}

	static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) {
		return {
			a.x + t * (b.x - a.x),
			a.y + t * (b.y - a.y)
		};
	}
};