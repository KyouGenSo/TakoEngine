#pragma once
#include "Collider.h"
#include "AABB.h"
#include "Mat4x4Func.h"

namespace Tako {

  /// <summary>
  /// 軸平行境界ボックス(Axis-Aligned Bounding Box)の衝突判定を行うコライダー
  /// </summary>
  class AABBCollider : public Collider {
  public: //メンバー関数
    AABBCollider() = default;
    virtual ~AABBCollider() = default;

    //===================================
    //Setter
    //===================================
    void SetOffset(const Vector3& offset) { offset_ = offset; }
    void SetSize(const Vector3& size) { size_ = size; }

    //===================================
    //Getter
    //===================================
    /// <summary>
    /// AABB の中心座標を取得（ワールド座標系）
    /// </summary>
    /// <returns>中心座標</returns>
    Vector3 GetCenter() const override;

    /// <summary>
    /// AABB 構造体を取得（ワールド座標系）
    /// </summary>
    /// <returns>AABB 構造体</returns>
    AABB GetAABB() const;

    const Vector3& GetOffset() const { return offset_; }
    const Vector3& GetSize() const { return size_; }

  private: //メンバー変数
    //形状
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };  ///< ローカル座標系でのオフセット
    Vector3 size_   = { 1.0f, 1.0f, 1.0f };  ///< ボックスのサイズ（幅、高さ、奥行き）
  };

} // namespace Tako