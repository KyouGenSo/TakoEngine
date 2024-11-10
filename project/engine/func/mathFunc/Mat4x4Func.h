#pragma once
#define _USE_MATH_DEFINES

#include "Matrix4x4.h"
#include "Vector3.h"

const int kRowHeight = 20;
const int kColumnWidth = 60;

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

namespace Mat4x4 {
	Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

	Matrix4x4 Subtrsct(const Matrix4x4& m1, const Matrix4x4& m2);

	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	Matrix4x4 Inverse(const Matrix4x4& m);

	Matrix4x4 Transpose(const Matrix4x4& m);

	Matrix4x4 MakeIdentity();

	Matrix4x4 MakeTranslate(const Vector3& translate);

	Matrix4x4 MakeScale(const Vector3& scale);

	Matrix4x4 MakeRotateX(float angle);

	Matrix4x4 MakeRotateY(float angle);

	Matrix4x4 MakeRotateZ(float angle);

	Matrix4x4 MakeRotateXYZ(Matrix4x4 mX, Matrix4x4 mY, Matrix4x4 mZ);

	Matrix4x4 MakeRotateXYZ(const Vector3& rotate);

	Matrix4x4 MakeAffine(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	Vector3 TransForm(const Matrix4x4& m, const Vector3& v);

	Matrix4x4 MakePerspective(float fovY, float aspectRatio, float nearClip, float farClip);

	Matrix4x4 MakeOrtho(float left, float top, float right, float bottom, float nearClip, float farClip);

	Matrix4x4 MakeViewport(float left, float top, float width, float height, float minDepth, float maxDepth);

	Matrix4x4 InverseTranspose(const Matrix4x4& m);
}




