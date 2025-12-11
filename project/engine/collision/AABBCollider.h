#pragma once
#include "Collider.h"
#include "AABB.h"
#include "Mat4x4Func.h"

namespace Tako {

/// <summary>
/// 軸平行境界ボックス(Axis-Aligned Bounding Box)の衝突判定を行うコライダー
/// </summary>
class AABBCollider : public Collider {
private:
	Vector3 offset_ = { 0.0f, 0.0f, 0.0f };  ///< ローカル座標系でのオフセット
	Vector3 size_ = { 1.0f, 1.0f, 1.0f };  ///< ボックスのサイズ（幅、高さ、奥行き）

public:
	AABBCollider() = default;
	virtual ~AABBCollider() = default;

	/// <summary>
	/// AABBの中心座標を取得（ワールド座標系）
	/// </summary>
	/// <returns>中心座標</returns>
	Vector3 GetCenter() const override;

	/// <summary>
	/// AABB構造体を取得（ワールド座標系）
	/// </summary>
	/// <returns>AABB構造体</returns>
	AABB GetAABB() const;

	/// <summary>
	/// ローカルオフセットを設定
	/// </summary>
	/// <param name="offset">オフセット値</param>
	void SetOffset(const Vector3& offset) { offset_ = offset; }

	/// <summary>
	/// ローカルオフセットを取得
	/// </summary>
	/// <returns>オフセット値</returns>
	const Vector3& GetOffset() const { return offset_; }

	/// <summary>
	/// AABBのサイズを設定
	/// </summary>
	/// <param name="size">サイズ（幅、高さ、奥行き）</param>
	void SetSize(const Vector3& size) { size_ = size; }

	/// <summary>
	/// AABBのサイズを取得
	/// </summary>
	/// <returns>サイズ（幅、高さ、奥行き）</returns>
	const Vector3& GetSize() const { return size_; }
};

} // namespace Tako