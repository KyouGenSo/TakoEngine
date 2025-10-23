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

/// <summary>
/// 衝突判定を一元管理するシングルトンマネージャー。コライダーの登録、衝突検出、衝突マスク管理を行う
/// </summary>
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
	
	/// <summary>
	/// シングルトンインスタンスを取得
	/// </summary>
	/// <returns>CollisionManagerのインスタンス</returns>
	static CollisionManager* GetInstance();

	/// <summary>
	/// シングルトンインスタンスを破棄
	/// </summary>
	static void Destroy();

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 全コライダーをクリアしリセット
	/// </summary>
	void Reset();

	/// <summary>
	/// 登録された全コライダーの衝突判定を実行
	/// </summary>
	void CheckAllCollisions();

	/// <summary>
	/// コライダーを登録
	/// </summary>
	/// <param name="collider">登録するコライダー</param>
	void AddCollider(Collider* collider);

	/// <summary>
	/// コライダーの登録を解除
	/// </summary>
	/// <param name="collider">解除するコライダー</param>
	void RemoveCollider(Collider* collider);

	/// <summary>
	/// 指定した型同士の衝突判定を有効/無効化
	/// </summary>
	/// <param name="typeA">型A</param>
	/// <param name="typeB">型B</param>
	/// <param name="canCollide">衝突判定を行う場合true</param>
	void SetCollisionMask(uint32_t typeA, uint32_t typeB, bool canCollide);

	/// <summary>
	/// 全コライダーのデバッグ描画
	/// </summary>
	void DrawColliders();

	/// <summary>
	/// ImGuiデバッグウィンドウの描画
	/// </summary>
	void DrawImGui();

  //-----------------------------Getters/Setters------------------------------//
	/// <summary>
	/// デバッグ描画の有効/無効を設定
	/// </summary>
	/// <param name="enabled">有効にする場合true</param>
	void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }

	/// <summary>
	/// デバッグ描画が有効かどうかを取得
	/// </summary>
	/// <returns>有効な場合true</returns>
	bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }

	/// <summary>
	/// 登録されているコライダー数を取得
	/// </summary>
	/// <returns>コライダー数</returns>
	size_t GetColliderCount() const { return colliders_.size(); }

	/// <summary>
	/// 登録されている全コライダーのリストを取得
	/// </summary>
	/// <returns>コライダーリストの参照</returns>
	const std::list<Collider*>& GetColliders() const { return colliders_; }

	/// <summary>
	/// 衝突マスク設定を取得
	/// </summary>
	/// <returns>衝突マスクマップの参照</returns>
	const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& GetCollisionMasks() const { return collisionMask_; }
	
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