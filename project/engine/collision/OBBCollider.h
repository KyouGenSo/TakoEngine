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
protected:
  Vector3 size_;           // ボックスのサイズ（幅、高さ、奥行き）
  Vector3 offset_;         // ローカルオフセット
  mutable Matrix4x4 orientation_;  // ローカル回転行列

public:
  OBBCollider();
  virtual ~OBBCollider() = default;

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

  /// <summary>
  /// OBB のサイズを設定
  /// </summary>
  /// <param name="size">サイズ（幅、高さ、奥行き）</param>
  void SetSize(const Vector3& size) { size_ = size; }

  /// <summary>
  /// OBB のサイズを取得
  /// </summary>
  /// <returns>サイズ（幅、高さ、奥行き）</returns>
  Vector3 GetSize() const { return size_; }

  /// <summary>
  /// ローカルオフセットを設定
  /// </summary>
  /// <param name="offset">オフセット値</param>
  void SetOffset(const Vector3& offset) { offset_ = offset; }

  /// <summary>
  /// ローカルオフセットを取得
  /// </summary>
  /// <returns>オフセット値</returns>
  Vector3 GetOffset() const { return offset_; }

  /// <summary>
  /// ローカル回転行列を設定
  /// </summary>
  /// <param name="orientation">回転行列</param>
  void SetOrientation(const Matrix4x4& orientation) { orientation_ = orientation; }

  /// <summary>
  /// ローカル回転行列を取得
  /// </summary>
  /// <returns>回転行列</returns>
  Matrix4x4 GetOrientation() const { return orientation_; }
};

} // namespace Tako