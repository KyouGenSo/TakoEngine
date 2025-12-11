#pragma once
#include "Collider.h"
#include "Mat4x4Func.h"

namespace Tako {

/// <summary>
/// 球体の衝突判定を行うコライダー
/// </summary>
class SphereCollider : public Collider {
private:
	Vector3 offset_ = { 0.0f, 0.0f, 0.0f };  ///< ローカル座標系でのオフセット
	float radius_ = 1.0f;  ///< 球の半径

public:
	SphereCollider() = default;
	virtual ~SphereCollider() = default;

	/// <summary>
	/// 球の中心座標を取得（ワールド座標系）
	/// </summary>
	/// <returns>中心座標</returns>
	Vector3 GetCenter() const override;

	/// <summary>
	/// 球の半径を取得
	/// </summary>
	/// <returns>半径</returns>
	float GetRadius() const { return radius_; }

	/// <summary>
	/// 球の半径を設定
	/// </summary>
	/// <param name="radius">半径</param>
	void SetRadius(float radius) { radius_ = radius; }

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
};

} // namespace Tako