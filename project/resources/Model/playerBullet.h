#pragma once
#include "Model.h"
#include "Audio.h"
#include "WorldTransform.h"
#include "Matrix4x4Function.h"
#include <Vector3Function.h>
#include <cassert>
#include "ImGuiManager.h"
#include "myFunction.h"
#include <algorithm>
#include "TextureManager.h"
#include "Collider.h"
#include "collisionTypeIdDef.h"
#include "collisionRecord.h"

class Enemy;

class PlayerBullet : public Collider {
public:
	PlayerBullet();
	~PlayerBullet();

	void Initialize(Model* model, const Vector3& position, const Vector3& velocity);
	void Update();
	void Draw(const ViewProjection& viewProjection);

	bool IsDead() const { return isDead_; }

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision([[maybe_unused]] Collider* other) override;

	/// <summary>
	/// 衝突記録をクリア
	/// </summary>
	void ClearCollisionRecord() { collisionRecord_.Clear(); }

	/// <summary>
	/// getter
	/// </summary>
	Vector3 GetWorldPosition() { return worldTransform_.translation_; }
	Vector3 GetCenter() const override;
	const uint32_t GetSerialNumber() const { return serialNumber_; }

	/// <summary>
	/// setter
	/// </summary>
	void SetEnemy(const Enemy* enemy) { enemy_ = enemy; }

private:
	Audio* audio_ = nullptr;

	// SE
	uint32_t seHit_ = 0u;

	WorldTransform worldTransform_;
	Model* model_ = nullptr;

	Vector3 velocity_;

	float t_ = 0.1f;

	static const int32_t kLifeTime = 60 * 5;
	int32_t deathTimer_ = kLifeTime;
	bool isDead_ = false;

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
	static uint32_t nextSerialNumber_;

	CollisionRecord collisionRecord_;

	// enemy
	const Enemy* enemy_ = nullptr;
};
