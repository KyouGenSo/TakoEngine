#pragma once
#include "Vector3.h"
#include "Quaternion.h"

namespace Tako {

/// <summary>
/// 3D 変換情報構造体
/// オイラー角による回転表現
/// </summary>
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

/// <summary>
/// 3D 変換情報構造体
/// クォータニオンによる回転表現
/// </summary>
struct QuatTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};

} // namespace Tako