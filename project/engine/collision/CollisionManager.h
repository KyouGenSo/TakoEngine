#pragma once
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <utility>
#include "Collider.h"

class AABBCollider;
class SphereCollider;
class OBBCollider;

class CollisionManager {
private:
	std::list<Collider*> colliders_;
	
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> collisionMask_;
	
	using CollisionPair = std::pair<Collider*, Collider*>;
	struct PairHash {
		size_t operator()(const CollisionPair& p) const {
			auto h1 = std::hash<void*>{}(p.first);
			auto h2 = std::hash<void*>{}(p.second);
			return h1 ^ (h2 << 1);
		}
	};
	std::unordered_set<CollisionPair, PairHash> previousCollisions_;
	std::unordered_set<CollisionPair, PairHash> currentCollisions_;
	
	static CollisionManager* instance_;
	
	bool debugDrawEnabled_ = false;

	CollisionManager() = default;

public:
	~CollisionManager() = default;
	
	static CollisionManager* GetInstance();
	static void Destroy();
	
	void Initialize();
	void Reset();
	void CheckAllCollisions();
	
	void AddCollider(Collider* collider);
	void RemoveCollider(Collider* collider);
	
	void SetCollisionMask(uint32_t typeA, uint32_t typeB, bool canCollide);
	
	// デバッグ描画
	void DrawColliders();
	void DrawImGui();
	void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }
	bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }
	
	// デバッグ用ゲッター
	size_t GetColliderCount() const { return colliders_.size(); }
	const std::list<Collider*>& GetColliders() const { return colliders_; }
	
private:
	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);
	bool CheckAABBvsAABB(AABBCollider* a, AABBCollider* b);
	bool CheckSphereVsSphere(SphereCollider* a, SphereCollider* b);
	bool CheckAABBvsSphere(AABBCollider* aabb, SphereCollider* sphere);
	bool CheckOBBvsOBB(OBBCollider* a, OBBCollider* b);
	bool CheckOBBvsAABB(OBBCollider* obb, AABBCollider* aabb);
	bool CheckOBBvsSphere(OBBCollider* obb, SphereCollider* sphere);
	
	bool CanCollide(uint32_t typeA, uint32_t typeB);
	CollisionPair MakeOrderedPair(Collider* a, Collider* b);
};