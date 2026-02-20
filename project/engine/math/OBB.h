#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include "Mat4x4Func.h"
#include <array>

namespace Tako {

  /// <summary>
  /// 有向境界ボックス(Oriented Bounding Box)。回転を含む境界ボックス。AABB より精密な衝突判定が可能
  /// </summary>
  struct OBB {
    Vector3 center;         // OBB の中心位置
    Vector3 halfExtents;    // 各軸の半サイズ（幅/2、高さ/2、奥行き/2）
    Matrix4x4 orientation;  // 回転行列（ローカル座標系の向き）

    // デフォルトコンストラクタ
    OBB() : center(0.0f, 0.0f, 0.0f), halfExtents(1.0f, 1.0f, 1.0f) {
      orientation = Mat4x4::MakeIdentity();
    }

    // コンストラクタ
    OBB(const Vector3& c, const Vector3& he, const Matrix4x4& orient)
      : center(c), halfExtents(he), orientation(orient) {
    }

    // OBB の軸ベクトルを取得（X 軸、Y 軸、Z 軸）
    Vector3 GetAxis(int index) const {
      switch (index) {
      case 0: return Vector3(orientation.m[0][0], orientation.m[0][1], orientation.m[0][2]).Normalize();
      case 1: return Vector3(orientation.m[1][0], orientation.m[1][1], orientation.m[1][2]).Normalize();
      case 2: return Vector3(orientation.m[2][0], orientation.m[2][1], orientation.m[2][2]).Normalize();
      default: return Vector3(0.0f, 0.0f, 0.0f);
      }
    }

    // OBB の8つの頂点を取得
    std::array<Vector3, 8> GetVertices() const {
      std::array<Vector3, 8> vertices;

      // ローカル空間での8つの頂点
      Vector3 localVertices[8] = {
        Vector3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vector3(halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vector3(halfExtents.x,  halfExtents.y, -halfExtents.z),
        Vector3(-halfExtents.x,  halfExtents.y, -halfExtents.z),
        Vector3(-halfExtents.x, -halfExtents.y,  halfExtents.z),
        Vector3(halfExtents.x, -halfExtents.y,  halfExtents.z),
        Vector3(halfExtents.x,  halfExtents.y,  halfExtents.z),
        Vector3(-halfExtents.x,  halfExtents.y,  halfExtents.z)
      };

      // ワールド空間に変換
      for (int i = 0; i < 8; ++i) {
        vertices[i] = Mat4x4::TransformNormal(orientation, localVertices[i]) + center;
      }

      return vertices;
    }
  };

} // namespace Tako