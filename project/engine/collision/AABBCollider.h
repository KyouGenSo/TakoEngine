#pragma once
#include "Collider.h"
#include "AABB.h"
#include "Mat4x4Func.h"

class AABBCollider : public Collider {
private:
	Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
	Vector3 size_ = { 1.0f, 1.0f, 1.0f };

public:
	AABBCollider() = default;
	virtual ~AABBCollider() = default;

	Vector3 GetCenter() const override;
	
	AABB GetAABB() const;
	
	void SetOffset(const Vector3& offset) { offset_ = offset; }
	const Vector3& GetOffset() const { return offset_; }
	
	void SetSize(const Vector3& size) { size_ = size; }
	const Vector3& GetSize() const { return size_; }
};