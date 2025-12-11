#pragma once

namespace Tako {

/// <summary>
/// 4x4行列
/// </summary>
struct Matrix4x4 final {
	float m[4][4];  ///< 行列要素 [行][列]で格納（行優先）

	/// <summary>
	/// 4x4行列同士の乗算
	/// </summary>
	/// <param name="mat">乗算する行列</param>
	/// <returns>乗算結果の行列</returns>
	Matrix4x4 operator*(const Matrix4x4& mat) const {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = this->m[i][0] * mat.m[0][j] + this->m[i][1] * mat.m[1][j] + this->m[i][2] * mat.m[2][j] + this->m[i][3] * mat.m[3][j];
			}
		}
		return result;
	}

	/// <summary>
	/// 4x4行列同士の乗算代入
	/// </summary>
	/// <param name="mat">乗算する行列</param>
	/// <returns>乗算後の自身の行列</returns>
	Matrix4x4 operator*= (const Matrix4x4& mat) {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = this->m[i][0] * mat.m[0][j] + this->m[i][1] * mat.m[1][j] + this->m[i][2] * mat.m[2][j] + this->m[i][3] * mat.m[3][j];
			}
		}
		return result;
	}
};

} // namespace Tako
