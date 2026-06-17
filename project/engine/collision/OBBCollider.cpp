#include "OBBCollider.h"

namespace Tako {

  OBBCollider::OBBCollider()
    : size_(1.0f, 1.0f, 1.0f)
    , offset_(0.0f, 0.0f, 0.0f) {
    orientation_ = Mat4x4::MakeIdentity();
  }

  OBB OBBCollider::GetOBB() const {
    OBB obb;

    Vector3 worldPos = transform_ ? transform_->translate : Vector3(0.0f, 0.0f, 0.0f);

    if (transform_) {
      // transform の回転で orientation_ を更新し、その回転でオフセットを移動
      orientation_ = Mat4x4::MakeRotateXYZ(transform_->rotate);
      Vector3 rotatedOffset = Mat4x4::TransformNormal(orientation_, offset_);
      obb.center = worldPos + rotatedOffset;
      obb.orientation = orientation_;
    }
    else {
      obb.center = worldPos + offset_;
      obb.orientation = orientation_;
    }

    obb.halfExtents = size_ * 0.5f;

    return obb;
  }

  Vector3 OBBCollider::GetCenter() const {
    if (transform_) {
      Vector3 rotatedOffset = Mat4x4::TransformNormal(orientation_, offset_);
      return transform_->translate + rotatedOffset;
    }
    return offset_;
  }

} // namespace Tako