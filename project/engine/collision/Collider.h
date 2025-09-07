#pragma once
#include "Vector3.h"
#include "Transform.h"

class Collider {
protected:
	Transform* transform_ = nullptr;
	uint32_t typeID_ = 0;
	bool isActive_ = true;
	void* owner_ = nullptr;

public:
	virtual ~Collider() = default;

	virtual Vector3 GetCenter() const = 0;
	
	virtual void OnCollisionEnter([[maybe_unused]] Collider* other) {}
	virtual void OnCollisionStay([[maybe_unused]] Collider* other) {}
	virtual void OnCollisionExit([[maybe_unused]] Collider* other) {}

	void SetTransform(Transform* transform) { transform_ = transform; }
	Transform* GetTransform() const { return transform_; }
	
	void SetActive(bool active) { isActive_ = active; }
	bool IsActive() const { return isActive_; }
	
	uint32_t GetTypeID() const { return typeID_; }
	void SetTypeID(uint32_t id) { typeID_ = id; }
	
	void SetOwner(void* owner) { owner_ = owner; }
	void* GetOwner() const { return owner_; }
};