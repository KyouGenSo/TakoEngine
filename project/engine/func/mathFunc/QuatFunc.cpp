#include "QuatFunc.h"
#include <cmath>

Quaternion Quat::Add(const Quaternion& q1, const Quaternion& q2)
{
	return Quaternion{ q1.x + q2.x, q1.y + q2.y, q1.z + q2.z, q1.w + q2.w };
}

Quaternion Quat::Subtract(const Quaternion& q1, const Quaternion& q2)
{
	return Quaternion{ q1.x - q2.x, q1.y - q2.y, q1.z - q2.z, q1.w - q2.w };
}

Quaternion Quat::Multiply(const Quaternion& q, float scaler)
{
	return Quaternion{ q.x * scaler, q.y * scaler, q.z * scaler, q.w * scaler };
}

Quaternion Quat::Multiply(const Quaternion& q1, const Quaternion& q2)
{
	Quaternion result;
	result.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
	result.y = q1.w * q2.y + q1.y * q2.w + q1.z * q2.x - q1.x * q2.z;
	result.z = q1.w * q2.z + q1.z * q2.w + q1.x * q2.y - q1.y * q2.x;
	result.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;

	return result;
}

float Quat::Norm(const Quaternion& q)
{
	return sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

Quaternion Quat::Normalize(const Quaternion& q)
{
	float norm = Norm(q);
	return Quaternion{ q.x / norm, q.y / norm, q.z / norm, q.w / norm };
}

Quaternion Quat::Conjugate(const Quaternion& q)
{
	return Quaternion{ -q.x, -q.y, -q.z, q.w };
}

Quaternion Quat::Inverse(const Quaternion& q)
{
	float norm = Norm(q);
	if (norm == 0.0f) // Normがゼロの場合は単位クォータニオンを返す
	{
		return Quaternion{ 0, 0, 0, 1 };
	}
	Quaternion conjugate = Conjugate(q);
	return Quaternion{ conjugate.x / norm, conjugate.y / norm, conjugate.z / norm, conjugate.w / norm };
}

Quaternion Quat::Slerp(const Quaternion& q1, const Quaternion& q2, float t)
{
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

    // ドット積が負の場合、片方のクォータニオンを反転
    Quaternion q2Modified = q2;
    if (dot < 0.0f)
    {
        dot = -dot;
        q2Modified = Quaternion{ -q2.x, -q2.y, -q2.z, -q2.w };
    }

    // 小さい角度の場合は線形補間を使用
    if (dot > 0.9995f)
    {
        Quaternion result;
        result.x = q1.x + t * (q2Modified.x - q1.x);
        result.y = q1.y + t * (q2Modified.y - q1.y);
        result.z = q1.z + t * (q2Modified.z - q1.z);
        result.w = q1.w + t * (q2Modified.w - q1.w);
        float length = sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
        result.x /= length;
        result.y /= length;
        result.z /= length;
        result.w /= length;
        return result;
    }

    float theta = acos(dot);
    float sinTheta = sin(theta);

    float sinThetaFrom = sin((1.0f - t) * theta);
    float sinThetaTo = sin(t * theta);

    Quaternion result;
    result.x = (q1.x * sinThetaFrom + q2Modified.x * sinThetaTo) / sinTheta;
    result.y = (q1.y * sinThetaFrom + q2Modified.y * sinThetaTo) / sinTheta;
    result.z = (q1.z * sinThetaFrom + q2Modified.z * sinThetaTo) / sinTheta;
    result.w = (q1.w * sinThetaFrom + q2Modified.w * sinThetaTo) / sinTheta;

    return result;
}

Vector3 Quat::ToVec3(const Quaternion& q)
{
    float sinY = 2 * (q.w * q.y - q.z * q.x);
    sinY = sinY > 1.0f ? 1.0f : (sinY < -1.0f ? -1.0f : sinY); // クランプ

    float x = atan2(2 * (q.w * q.x + q.y * q.z), 1 - 2 * (q.x * q.x + q.y * q.y));
    float y = asin(sinY);
    float z = atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z));

    return Vector3{ x, y, z };
}

Matrix4x4 Quat::ToMatrix(const Quaternion& q)
{
	Matrix4x4 mat;
	float xx = q.x * q.x;
	float yy = q.y * q.y;
	float zz = q.z * q.z;
	float xy = q.x * q.y;
	float xz = q.x * q.z;
	float yz = q.y * q.z;
	float wx = q.w * q.x;
	float wy = q.w * q.y;
	float wz = q.w * q.z;

	mat.m[0][0] = 1.0f - 2.0f * (yy + zz);
	mat.m[0][1] = 2.0f * (xy - wz);
	mat.m[0][2] = 2.0f * (xz + wy);
	mat.m[0][3] = 0.0f;

	mat.m[1][0] = 2.0f * (xy + wz);
	mat.m[1][1] = 1.0f - 2.0f * (xx + zz);
	mat.m[1][2] = 2.0f * (yz - wx);
	mat.m[1][3] = 0.0f;

	mat.m[2][0] = 2.0f * (xz - wy);
	mat.m[2][1] = 2.0f * (yz + wx);
	mat.m[2][2] = 1.0f - 2.0f * (xx + yy);
	mat.m[2][3] = 0.0f;

	mat.m[3][0] = 0.0f;
	mat.m[3][1] = 0.0f;
	mat.m[3][2] = 0.0f;
	mat.m[3][3] = 1.0f;

	return mat;
}
