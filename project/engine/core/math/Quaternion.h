#pragma once
#include "Vec3Func.h"

namespace Tako {

  /// <summary>
  /// クォータニオン。3D 回転を表現する数学的構造体
  /// </summary>
  struct Quaternion {
    float x;  ///< 虚部の X 成分
    float y;  ///< 虚部の Y 成分
    float z;  ///< 虚部の Z 成分
    float w;  ///< 実部
  };

} // namespace Tako