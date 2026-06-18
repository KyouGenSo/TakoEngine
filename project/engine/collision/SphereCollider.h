#pragma once
#include "Collider.h"
#include "Mat4x4Func.h"

namespace Tako {

  /// <summary>
  /// 球体の衝突判定を行うコライダー
  /// </summary>
  class SphereCollider : public Collider {
  public: //メンバー関数
    SphereCollider() = default;
    virtual ~SphereCollider() = default;

    //===================================
    //Setter
    //===================================
    void SetRadius(float radius) { radius_ = radius; }
    void SetOffset(const Vector3& offset) { offset_ = offset; }

    //===================================
    //Getter
    //===================================
    /// <summary>
    /// 球の中心座標を取得（ワールド座標系）
    /// </summary>
    /// <returns>中心座標</returns>
    Vector3 GetCenter() const override;

    float GetRadius() const { return radius_; }
    const Vector3& GetOffset() const { return offset_; }

  private: //メンバー変数
    //形状
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };  ///< ローカル座標系でのオフセット
    float   radius_ = 1.0f;                  ///< 球の半径
  };

} // namespace Tako