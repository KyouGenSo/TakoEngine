#include "SphereCollider.h"

namespace Tako {

Vector3 SphereCollider::GetCenter() const {
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

} // namespace Tako