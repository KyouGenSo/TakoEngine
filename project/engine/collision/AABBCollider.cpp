#include "AABBCollider.h"

namespace Tako {

Vector3 AABBCollider::GetCenter() const {
	if (!transform_) {
		return offset_;
	}
	
	Matrix4x4 worldMatrix = Mat4x4::MakeAffine(
		transform_->scale,
		transform_->rotate, 
		transform_->translate
	);
	
	Vector3 worldPos = Mat4x4::Transform(worldMatrix, offset_);
	return worldPos;
}

AABB AABBCollider::GetAABB() const {
	Vector3 center = GetCenter();
	
	AABB aabb;
	aabb.min = center - size_ / 2;
	aabb.max = center + size_ / 2;
	
	return aabb;
}

} // namespace Tako