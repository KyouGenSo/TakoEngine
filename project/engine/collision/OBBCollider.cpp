#include "OBBCollider.h"

namespace Tako {

OBBCollider::OBBCollider()
	: size_(1.0f, 1.0f, 1.0f)
	, offset_(0.0f, 0.0f, 0.0f) {
  orientation_ = Mat4x4::MakeIdentity();
}

OBB OBBCollider::GetOBB() const {
	OBB obb;
	
	// ワールド座標での中心位置を計算
	Vector3 worldPos = transform_ ? transform_->translate : Vector3(0.0f, 0.0f, 0.0f);
	
	// オフセットを回転を考慮して適用
	if (transform_) {
		// orientation_が設定されている場合はそれを使用
		// オフセットをorientation_で回転
    orientation_ = Mat4x4::MakeRotateXYZ(transform_->rotate);
		Vector3 rotatedOffset = Mat4x4::TransformNormal(orientation_, offset_);
		obb.center = worldPos + rotatedOffset;
		// orientation_をそのまま使用
		obb.orientation = orientation_;
	} else {
		obb.center = worldPos + offset_;
		obb.orientation = orientation_;
	}
	
	// 半サイズを設定
	obb.halfExtents = size_ * 0.5f;
	
	return obb;
}

Vector3 OBBCollider::GetCenter() const {
	if (transform_) {
		// orientation_を使用してオフセットを回転
		Vector3 rotatedOffset = Mat4x4::TransformNormal(orientation_, offset_);
		return transform_->translate + rotatedOffset;
	}
	return offset_;
}

} // namespace Tako