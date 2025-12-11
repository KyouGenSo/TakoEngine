#pragma once
#include <limits>
#include <cmath>

namespace Tako {

/// <summary>
/// 3次元ベクトル
/// </summary>
struct Vector3 final {
	float x;  ///< X座標
	float y;  ///< Y座標
	float z;  ///< Z座標

	/// <summary>
	/// ベクトルの加算代入演算子
	/// </summary>
	/// <param name="v">加算するベクトル</param>
	/// <returns>自身の参照</returns>
	Vector3& operator+=(const Vector3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	/// <summary>
	/// ベクトルの減算代入演算子
	/// </summary>
	/// <param name="v">減算するベクトル</param>
	/// <returns>自身の参照</returns>
	Vector3& operator-=(const Vector3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	/// <summary>
	/// ベクトルのスカラー乗算代入演算子
	/// </summary>
	/// <param name="s">乗算するスカラー値</param>
	/// <returns>自身の参照</returns>
	Vector3& operator*=(float s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}

	/// <summary>
	/// ベクトルのスカラー除算代入演算子
	/// </summary>
	/// <param name="s">除算するスカラー値</param>
	/// <returns>自身の参照</returns>
	Vector3& operator/=(float s) {
		x /= s;
		y /= s;
		z /= s;
		return *this;
	}

	/// <summary>
	/// ベクトルの加算演算子
	/// </summary>
	/// <param name="v">加算するベクトル</param>
	/// <returns>加算結果のベクトル</returns>
	Vector3 operator+(const Vector3& v) const {
		Vector3 result;
		result.x = x + v.x;
		result.y = y + v.y;
		result.z = z + v.z;

		return result;
	}

	/// <summary>
	/// ベクトルの減算演算子
	/// </summary>
	/// <param name="v">減算するベクトル</param>
	/// <returns>減算結果のベクトル</returns>
	Vector3 operator-(const Vector3& v) const {
		Vector3 result;
		result.x = x - v.x;
		result.y = y - v.y;
		result.z = z - v.z;

		return result;
	}

	/// <summary>
	/// ベクトルのスカラー乗算演算子
	/// </summary>
	/// <param name="s">乗算するスカラー値</param>
	/// <returns>乗算結果のベクトル</returns>
	Vector3 operator*(float s) const {
		Vector3 result;
		result.x = x * s;
		result.y = y * s;
		result.z = z * s;

		return result;
	}

	/// <summary>
	/// スカラーとベクトルの乗算演算子（フレンド関数）
	/// </summary>
	/// <param name="s">乗算するスカラー値</param>
	/// <param name="v">乗算するベクトル</param>
	/// <returns>乗算結果のベクトル</returns>
	friend Vector3 operator*(float s, const Vector3& v) {
		return v * s;
	}

	/// <summary>
	/// ベクトルのスカラー除算演算子
	/// </summary>
	/// <param name="s">除算するスカラー値</param>
	/// <returns>除算結果のベクトル</returns>
	Vector3 operator/(float s) const {
		Vector3 result;
		result.x = x / s;
		result.y = y / s;
		result.z = z / s;

		return result;
	}

	/// <summary>
	/// ベクトルの単項マイナス演算子（符号反転）
	/// </summary>
	/// <returns>符号反転したベクトル</returns>
	Vector3 operator-() const {
		return {-x, -y, -z};
	}

	/// <summary>
	/// ベクトルの等価比較演算子（誤差を考慮）
	/// </summary>
	/// <param name="v">比較するベクトル</param>
	/// <returns>等しい場合true、異なる場合false</returns>
	bool operator==(const Vector3& v) const {
		return (std::abs(x - v.x) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(y - v.y) <= std::numeric_limits<float>::epsilon() &&
		        std::abs(z - v.z) <= std::numeric_limits<float>::epsilon());
	}

	/// <summary>
	/// ベクトルの非等価比較演算子
	/// </summary>
	/// <param name="v">比較するベクトル</param>
	/// <returns>異なる場合true、等しい場合false</returns>
	bool operator!=(const Vector3& v) const {
		return !(*this == v);
	}

	/// <summary>
	/// ベクトルの正規化（長さを1にする）
	/// </summary>
	/// <returns>正規化されたベクトル（ゼロベクトルの場合はゼロベクトルを返す）</returns>
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

	/// <summary>
	/// ベクトルの長さ（大きさ）を計算
	/// </summary>
	/// <returns>ベクトルの長さ</returns>
	float Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	/// <summary>
	/// ベクトルの長さの2乗を計算（平方根計算を省略して高速化）
	/// </summary>
	/// <returns>ベクトルの長さの2乗</returns>
	float LengthSquared() const {
		return x * x + y * y + z * z;
	}

	/// <summary>
	/// ベクトルの内積（ドット積）を計算
	/// </summary>
	/// <param name="v">内積を計算する相手のベクトル</param>
	/// <returns>内積の値</returns>
	float Dot(const Vector3& v) const {
		return x * v.x + y * v.y + z * v.z;
	}

	/// <summary>
	/// ベクトルの外積（クロス積）を計算
	/// </summary>
	/// <param name="v">外積を計算する相手のベクトル</param>
	/// <returns>外積ベクトル（2つのベクトルに垂直なベクトル）</returns>
	Vector3 Cross(const Vector3& v) const {
		return {
			y * v.z - z * v.y,
			z * v.x - x * v.z,
			x * v.y - y * v.x
		};
	}

	/// <summary>
	/// 2つのベクトル間の距離を計算
	/// </summary>
	/// <param name="v">距離を計算する相手のベクトル</param>
	/// <returns>2点間の距離</returns>
	float Distance(const Vector3& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		float dz = z - v.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	/// <summary>
	/// 2つのベクトル間の距離の2乗を計算（平方根計算を省略して高速化）
	/// </summary>
	/// <param name="v">距離を計算する相手のベクトル</param>
	/// <returns>2点間の距離の2乗</returns>
	float DistanceSquared(const Vector3& v) const {
		float dx = x - v.x;
		float dy = y - v.y;
		float dz = z - v.z;
		return dx * dx + dy * dy + dz * dz;
	}

	/// <summary>
	/// 2つのベクトル間の線形補間
	/// </summary>
	/// <param name="a">開始ベクトル</param>
	/// <param name="b">終了ベクトル</param>
	/// <param name="t">補間パラメータ（0.0〜1.0）</param>
	/// <returns>補間されたベクトル</returns>
	static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
		return {
			a.x + t * (b.x - a.x),
			a.y + t * (b.y - a.y),
			a.z + t * (b.z - a.z)
		};
	}

	/// <summary>
	/// 法線ベクトルに対する反射ベクトルを計算
	/// </summary>
	/// <param name="normal">反射面の法線ベクトル（正規化されている必要がある）</param>
	/// <returns>反射ベクトル</returns>
	Vector3 Reflect(const Vector3& normal) const {
		float dot = this->Dot(normal);
		return *this - normal * (2.0f * dot);
	}

	/// <summary>
	/// ベクトルを別のベクトルに射影（投影）
	/// </summary>
	/// <param name="onto">射影先のベクトル</param>
	/// <returns>射影されたベクトル（ontoと平行なベクトル）</returns>
	Vector3 Project(const Vector3& onto) const {
		float lengthSquared = onto.LengthSquared();
		if (lengthSquared <= std::numeric_limits<float>::epsilon()) {
			return {0.0f, 0.0f, 0.0f};
		}
		float scalar = this->Dot(onto) / lengthSquared;
		return onto * scalar;
	}
};

} // namespace Tako
