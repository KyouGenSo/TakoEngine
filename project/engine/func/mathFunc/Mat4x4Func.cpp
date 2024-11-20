#include"Mat4x4Func.h"
#include"math.h"
#include"cmath"
#include"cassert"
#include "QuatFunc.h"

#define _USE_MATH_DEFINES


namespace Mat4x4 {
	Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = m1.m[i][j] + m2.m[i][j];
			}
		}
		return result;
	}

	Matrix4x4 Subtrsct(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = m1.m[i][j] - m2.m[i][j];
			}
		}
		return result;
	}

	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = m1.m[i][0] * m2.m[0][j] + m1.m[i][1] * m2.m[1][j] + m1.m[i][2] * m2.m[2][j] + m1.m[i][3] * m2.m[3][j];
			}
		}
		return result;
	}

	Matrix4x4 Inverse(const Matrix4x4& m) {
		Matrix4x4 result;

		float det =
			m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]
			+ m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]
			+ m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]
			+ m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]
			- m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2] - m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]
			- m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]
			- m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]
			- m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1];

		if (fabs(det) < 0.0001f) {
			return result;
		}

		float invDet = 1.0f / det;

		result.m[0][0] = (m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][3] * m.m[3][2] - m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][3] * m.m[2][2] * m.m[3][1]) * invDet;
		result.m[0][1] = (m.m[0][1] * m.m[2][3] * m.m[3][2] + m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][3] * m.m[2][2] * m.m[3][1] - m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2]) * invDet;
		result.m[0][2] = (m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][3] * m.m[3][2] - m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][3] * m.m[1][2] * m.m[3][1]) * invDet;
		result.m[0][3] = (m.m[0][1] * m.m[1][3] * m.m[2][2] + m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][3] * m.m[1][2] * m.m[2][1] - m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2]) * invDet;
		result.m[1][0] = (m.m[1][0] * m.m[2][3] * m.m[3][2] + m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][3] * m.m[2][2] * m.m[3][0] - m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2]) * invDet;
		result.m[1][1] = (m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][3] * m.m[3][2] - m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][3] * m.m[2][2] * m.m[3][0]) * invDet;
		result.m[1][2] = (m.m[0][0] * m.m[1][3] * m.m[3][2] + m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][3] * m.m[1][2] * m.m[3][0] - m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2]) * invDet;
		result.m[1][3] = (m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][3] * m.m[2][2] - m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][3] * m.m[1][2] * m.m[2][0]) * invDet;
		result.m[2][0] = (m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][3] * m.m[2][1] * m.m[3][0]) * invDet;
		result.m[2][1] = (m.m[0][0] * m.m[2][3] * m.m[3][1] + m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][3] * m.m[2][1] * m.m[3][0] - m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1]) * invDet;
		result.m[2][2] = (m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][0] * m.m[1][3] * m.m[3][1] - m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][3] * m.m[1][1] * m.m[3][0]) * invDet;
		result.m[2][3] = (m.m[0][0] * m.m[1][3] * m.m[2][1] + m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][3] * m.m[1][1] * m.m[2][0] - m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1]) * invDet;
		result.m[3][0] = (m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][2] * m.m[2][1] * m.m[3][0] - m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1]) * invDet;
		result.m[3][1] = (m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][0] * m.m[2][2] * m.m[3][1] - m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][2] * m.m[2][1] * m.m[3][0]) * invDet;
		result.m[3][2] = (m.m[0][0] * m.m[1][2] * m.m[3][1] + m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][2] * m.m[1][1] * m.m[3][0] - m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1]) * invDet;
		result.m[3][3] = (m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] - m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][2] * m.m[1][1] * m.m[2][0]) * invDet;

		return result;

	}

	Matrix4x4 Transpose(const Matrix4x4& m) {
		Matrix4x4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = m.m[j][i];
			}
		}
		return result;
	}

	Matrix4x4 MakeIdentity() {
		Matrix4x4 result;

		result.m[0][0] = 1.0f;
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;
		result.m[1][0] = 0.0f;
		result.m[1][1] = 1.0f;
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;
		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = 1.0f;
		result.m[2][3] = 0.0f;
		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 MakeTranslate(const Vector3& translate) {
		Matrix4x4 result = MakeIdentity();

		result.m[3][0] = translate.x;
		result.m[3][1] = translate.y;
		result.m[3][2] = translate.z;

		return result;
	}

	Matrix4x4 MakeScale(const Vector3& scale) {
		Matrix4x4 result = MakeIdentity();

		result.m[0][0] = scale.x;
		result.m[1][1] = scale.y;
		result.m[2][2] = scale.z;

		return result;
	}

	Matrix4x4 MakeRotateX(float angle) {
		Matrix4x4 result = MakeIdentity();

		float s = std::sin(angle);
		float c = std::cos(angle);

		result.m[1][1] = c;
		result.m[1][2] = s;
		result.m[2][1] = -s;
		result.m[2][2] = c;

		return result;
	}

	Matrix4x4 MakeRotateY(float angle) {
		Matrix4x4 result = MakeIdentity();

		float s = std::sin(angle);
		float c = std::cos(angle);

		result.m[0][0] = c;
		result.m[0][2] = -s;
		result.m[2][0] = s;
		result.m[2][2] = c;

		return result;
	}

	Matrix4x4 MakeRotateZ(float angle) {
		Matrix4x4 result = MakeIdentity();

		float s = std::sin(angle);
		float c = std::cos(angle);

		result.m[0][0] = c;
		result.m[0][1] = s;
		result.m[1][0] = -s;
		result.m[1][1] = c;

		return result;
	}

	Matrix4x4 MakeRotateXYZ(Matrix4x4 mX, Matrix4x4 mY, Matrix4x4 mZ) {
		Matrix4x4 result = Multiply(mX, Multiply(mY, mZ));

		return result;
	}

	Matrix4x4 MakeRotateXYZ(const Vector3& rotate)
	{
		Matrix4x4 result = MakeRotateXYZ(MakeRotateX(rotate.x), MakeRotateY(rotate.y), MakeRotateZ(rotate.z));

		return result;
	}

	Matrix4x4 MakeAffine(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
		Matrix4x4 result = MakeScale(scale);
		result = Multiply(result, MakeRotateXYZ(MakeRotateX(rotate.x), MakeRotateY(rotate.y), MakeRotateZ(rotate.z)));
		result = Multiply(result, MakeTranslate(translate));

		return result;
	}

	Matrix4x4 MakeAffine(const Vector3& scale, const Quaternion& rotate, const Vector3& translate)
	{
		Matrix4x4 result = MakeScale(scale);
		result = Multiply(result, Quat::ToMatrix(rotate));
		result = Multiply(result, MakeTranslate(translate));

		return result;

	}

	Vector3 TransForm(const Matrix4x4& m, const Vector3& v) {
		Vector3 result;
		float w;

		result.x = m.m[0][0] * v.x + m.m[1][0] * v.y + m.m[2][0] * v.z + m.m[3][0];
		result.y = m.m[0][1] * v.x + m.m[1][1] * v.y + m.m[2][1] * v.z + m.m[3][1];
		result.z = m.m[0][2] * v.x + m.m[1][2] * v.y + m.m[2][2] * v.z + m.m[3][2];
		w = m.m[0][3] * v.x + m.m[1][3] * v.y + m.m[2][3] * v.z + m.m[3][3];

		assert(w != 0.0f);

		result.x /= w;
		result.y /= w;
		result.z /= w;

		return result;
	}

	Matrix4x4 MakePerspective(float fovY, float aspectRatio, float nearClip, float farClip) {
		Matrix4x4 result;

		float f = 1 / std::tan(fovY / 2.0f);
		float range = farClip - nearClip;

		result.m[0][0] = (1 / aspectRatio) * f;
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = f;
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = farClip / range;
		result.m[2][3] = 1.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = -nearClip * farClip / range;
		result.m[3][3] = 0.0f;

		return result;
	}

	Matrix4x4 MakeOrtho(float left, float top, float right, float bottom, float nearClip, float farClip) {
		Matrix4x4 result;

		result.m[0][0] = 2.0f / (right - left);
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = 2.0f / (top - bottom);
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[2][3] = 0.0f;

		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 MakeViewport(float left, float top, float width, float height, float minDepth, float maxDepth) {
		Matrix4x4 result;

		result.m[0][0] = width / 2.0f;
		result.m[0][1] = 0.0f;
		result.m[0][2] = 0.0f;
		result.m[0][3] = 0.0f;

		result.m[1][0] = 0.0f;
		result.m[1][1] = -height / 2.0f;
		result.m[1][2] = 0.0f;
		result.m[1][3] = 0.0f;

		result.m[2][0] = 0.0f;
		result.m[2][1] = 0.0f;
		result.m[2][2] = maxDepth - minDepth;
		result.m[2][3] = 0.0f;

		result.m[3][0] = left + width / 2.0f;
		result.m[3][1] = top + height / 2.0f;
		result.m[3][2] = minDepth;
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 InverseTranspose(const Matrix4x4& m)
	{
		Matrix4x4 result = Transpose(Inverse(m));

		return result;
	}
}
