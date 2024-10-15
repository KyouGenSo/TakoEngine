#pragma once
#include "Vector3.h"

/// <summary>
/// 3x3行列
/// </summary>
struct Matrix3x3 final {
	float m[3][3];

	// 演算子のオーバーロード

	/// <summary>
	/// 3x3行列同士の乗算
	/// </summary>
	Matrix3x3 operator*(const Matrix3x3& m) const {
		Matrix3x3 result;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				result.m[i][j] = this->m[i][0] * m.m[0][j] + this->m[i][1] * m.m[1][j] + this->m[i][2] * m.m[2][j];
			}
		}
		return result;
	}

	/// <summary>
	/// 3x3行列と3次元ベクトルの乗算
	/// </summary>
	Vector3 operator*(const Vector3& v) const {
		Vector3 result;
		result.x = this->m[0][0] * v.x + this->m[0][1] * v.y + this->m[0][2] * v.z;
		result.y = this->m[1][0] * v.x + this->m[1][1] * v.y + this->m[1][2] * v.z;
		result.z = this->m[2][0] * v.x + this->m[2][1] * v.y + this->m[2][2] * v.z;
		return result;
	}
};