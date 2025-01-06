#pragma once
#include "Vector3.h"
#include "Quaternion.h"
#include "Mat4x4Func.h"


class Transform {
public:
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
	const Transform* parent_ = nullptr;
	Matrix4x4 parentWorldMatrix_;

	Transform() : scale(Vector3(1.0f, 1.0f, 1.0f)), rotate(Vector3(0.0f, 0.0f, 0.0f)), translate(Vector3(0.0f, 0.0f, 0.0f)) {}

	void SetParent(const Transform* parent) {
		this->parent_ = parent;
	}

	bool HasParent() const {
		return parent_ != nullptr;
	}

	void Update() {
		if (HasParent()) {
			parentWorldMatrix_ = Mat4x4::MakeAffine(parent_->scale, parent_->rotate, parent_->translate);
			//parentWorldMatrix_ = Mat4x4::Multiply(Mat4x4::MakeAffine(scale, rotate, translate), parentWorldMatrix_);
			if (parent_->HasParent()) {
				parentWorldMatrix_ = Mat4x4::Multiply(parentWorldMatrix_, parent_->parentWorldMatrix_);
			}
		} else
		{
			parentWorldMatrix_ = Mat4x4::MakeIdentity();
		}
	}
};

struct QuatTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};