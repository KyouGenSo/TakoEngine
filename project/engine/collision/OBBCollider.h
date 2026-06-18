#pragma once
#include "Collider.h"
#include "OBB.h"
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Tako {

  /// <summary>
  /// 有向境界ボックス(Oriented Bounding Box)の衝突判定を行うコライダー
  /// </summary>
  class OBBCollider : public Collider {
  public: //メンバー関数
    OBBCollider();
    virtual ~OBBCollider() = default;

    //===============================================
    //Setter
    //===============================================
    void SetSize(const Vector3& size) { size_ = size; }
    void SetOffset(const Vector3& offset) { offset_ = offset; }
    void SetOrientation(const Matrix4x4& orientation) { orientation_ = orientation; }

    //===============================================
    //Getter
    //===============================================
    /// <summary>
    /// OBB 構造体を取得（ワールド座標系）
    /// </summary>
    /// <returns>OBB 構造体</returns>
    OBB GetOBB() const;

    /// <summary>
    /// OBB の中心座標を取得（ワールド座標系）
    /// </summary>
    /// <returns>中心座標</returns>
    Vector3 GetCenter() const override;

    Vector3 GetSize() const { return size_; }
    Vector3 GetOffset() const { return offset_; }
    Matrix4x4 GetOrientation() const { return orientation_; }

  protected: //メンバー変数
    //形状
    Vector3           size_;         ///< ボックスのサイズ（幅、高さ、奥行き）
    Vector3           offset_;       ///< ローカルオフセット
    mutable Matrix4x4 orientation_;  ///< ローカル回転行列
  };

} // namespace Tako