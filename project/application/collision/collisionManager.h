#pragma once
#include "Vector3.h"
#include "collider.h"
#include "list"
#include "Object3d.h"
#include <memory>

class CollisionManager {
private:
	std::list<Collider*> colliders_;

	std::unique_ptr<Object3d> collBallModel_;

	bool collBallVisibility_ = false;

public:
	void Initialize();

	void UpdateWorldTransform();

	void Draw();

	void ImGuiDraw();

	void Reset();

	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

	void CheckAllCollisions();

	void AddCollider(Collider* collider) {
		colliders_.push_back(collider);
	}

	void SetCollBallVisibility(bool visibility) { collBallVisibility_ = visibility; }
};