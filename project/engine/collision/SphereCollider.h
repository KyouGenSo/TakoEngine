#pragma once
#include "Collider.h"
#include "Mat4x4Func.h"

class SphereCollider : public Collider {
private:
	Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
	float radius_ = 1.0f;

public:
	SphereCollider() = default;
	virtual ~SphereCollider() = default;

	Vector3 GetCenter() const override;
	
	float GetRadius() const { return radius_; }
	void SetRadius(float radius) { radius_ = radius; }
	
	void SetOffset(const Vector3& offset) { offset_ = offset; }
	const Vector3& GetOffset() const { return offset_; }
};