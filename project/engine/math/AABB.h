#pragma once
#include "Vector3.h"

namespace Tako {

/// <summary>
/// 軸平行境界ボックス(Axis-Aligned Bounding Box)。衝突判定や範囲チェックに使用
/// </summary>
struct AABB {
	Vector3 min;  ///< ボックスの最小座標（左下後ろ）
	Vector3 max;  ///< ボックスの最大座標（右上前）
};

} // namespace Tako
