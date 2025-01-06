#pragma once
#include "Audio.h"
#include "Collider.h"
#include "Mat4x4Func.h"
#include "Model.h"
#include "cassert"
#include "collisionRecord.h"
#include "collisionTypeIdDef.h"
#include <Vec3Func.h>

class FollowCamera;

class EnemyBlock : public Collider {
public:
	EnemyBlock();
	~EnemyBlock();

	void Initialize(Object3d* model, const Vector3& position, const Vector3& scale, const Vector3& offset);
	void Update();
	void Draw();

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision([[maybe_unused]] Collider* other) override;

	/// <summary>
	/// 衝突記録をクリア
	/// </summary>
	void ClearCollisionRecord() { collisionRecord_.Clear(); }

	void SetColliVanish(bool isColliVanish) { isColliVanish_ = isColliVanish; }

	/// <summary>
	/// getter
	/// </summary>
	Vector3 GetTranslation() { return worldTransform_.translate; }
	Vector3 GetWorldPosition();
	Vector3 GetRotation() { return worldTransform_.rotate; }
	Vector3 GetScale() { return worldTransform_.scale; }
	Vector3 GetCenter() const override;
	// get offset
	const Vector3& GetOffset() const { return offset_; }
	// get isHit
	bool IsHit() const { return isHit_; }
	// get isColliVanish
	bool IsColliVanish() const { return isColliVanish_; }

	bool IsDead() const { return isDead_; }
	float GetDeathTimer() const { return deathTimer_; }

	/// <summary>
	/// setter
	/// </summary>
	void SetPosition(const Vector3& position) { worldTransform_.translate = position; }
	void SetPositionX(float positionX) { worldTransform_.translate.x = positionX; }
	void SetPositionY(float positionY) { worldTransform_.translate.y = positionY; }
	void SetPositionZ(float positionZ) { worldTransform_.translate.z = positionZ; }
	void SetRotation(const Vector3& rotation) { worldTransform_.rotate = rotation; }
	void SetRotationX(float rotationX) { worldTransform_.rotate.x = rotationX; }
	void SetRotationY(float rotationY) { worldTransform_.rotate.y = rotationY; }
	void SetRotationZ(float rotationZ) { worldTransform_.rotate.z = rotationZ; }
	void SetScale(const Vector3& scale) { worldTransform_.scale = scale; }
	void SetScaleX(float scaleX) { worldTransform_.scale.x = scaleX; }
	void SetScaleY(float scaleY) { worldTransform_.scale.y = scaleY; }
	void SetScaleZ(float scaleZ) { worldTransform_.scale.z = scaleZ; }
	void SetOffset(const Vector3& offset) { offset_ = offset; }
	void SetIsDead(bool isDead) { isDead_ = isDead; }
	// set death timer
	void SetDeathTimer(float deathTimer) { deathTimer_ = deathTimer; }
	// set damage
	void SetDamage(float damage) { damage_ = damage; }
	// set hited
	void SetIsHited(bool isHited) { isHited_ = isHited; }
	// set hit once
	void SetHitOnce(bool hitOnce) { hitOnce_ = hitOnce; }
	// set follow camera
	void SetFollowCamera(FollowCamera* followCamera) { followCamera_ = followCamera; }

private:
	Audio* audio_ = nullptr;

	// SE
	uint32_t seHandle_ = 0;

	Transform worldTransform_;
	Object3d* model_ = nullptr;

	float deathTimer_ = 60 * 5;
	bool isDead_ = false;

	bool isColliVanish_ = true;

	Vector3 offset_ = {0.0f, 0.0f, 0.0f};

	float damage_ = 1.0f;

	bool isHited_ = false;
	bool hitOnce_ = false;
	bool isHit_ = false;

	CollisionRecord collisionRecord_;

	// follow camera
	FollowCamera* followCamera_ = nullptr;
};