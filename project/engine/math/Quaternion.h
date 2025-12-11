#pragma once
#include "Vec3Func.h"

namespace Tako {

/// <summary>
/// クォータニオン。3D回転を表現する数学的構造体
/// </summary>
struct Quaternion {
	float x;  ///< 虚部のX成分
	float y;  ///< 虚部のY成分
	float z;  ///< 虚部のZ成分
	float w;  ///< 実部

	/// <summary>
	/// クォータニオン同士の乗算代入（回転の合成）
	/// </summary>
	/// <param name="q">合成するクォータニオン</param>
	/// <returns>合成後のクォータニオン</returns>
	Quaternion operator*=(const Quaternion& q) {
		Quaternion result;
		result.x = w * q.x + x * q.w + y * q.z - z * q.y;
		result.y = w * q.y + y * q.w + z * q.x - x * q.z;
		result.z = w * q.z + z * q.w + x * q.y - y * q.x;
		result.w = w * q.w - x * q.x - y * q.y - z * q.z;

		return result;
	}

	/// <summary>
	/// クォータニオン同士の乗算（回転の合成）
	/// </summary>
	/// <param name="q">合成するクォータニオン</param>
	/// <returns>合成後の新しいクォータニオン</returns>
	Quaternion operator*(const Quaternion& q) const {
		Quaternion result;
		result.x = w * q.x + x * q.w + y * q.z - z * q.y;
		result.y = w * q.y + y * q.w + z * q.x - x * q.z;
		result.z = w * q.z + z * q.w + x * q.y - y * q.x;
		result.w = w * q.w - x * q.x - y * q.y - z * q.z;

		return result;
	}
};

} // namespace Tako