#pragma once
/// <summary>
/// 4x4行列
/// </summary>
struct Matrix4x4 final {
	float m[4][4];

	Matrix4x4 operator*(const Matrix4x4& mat) const {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = this->m[i][0] * mat.m[0][j] + this->m[i][1] * mat.m[1][j] + this->m[i][2] * mat.m[2][j] + this->m[i][3] * mat.m[3][j];
			}
		}
		return result;
	}

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
