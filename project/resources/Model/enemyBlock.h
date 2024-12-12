#pragma once
#include "Audio.h"
#include "Collider.h"
#include "Matrix4x4Function.h"
#include "Model.h"
#include "ViewProjection.h"
#include "WorldTransform.h"
#include "cassert"
#include "collisionRecord.h"
#include "collisionTypeIdDef.h"
#include "myFunction.h"
#include <Vector3Function.h>

class FollowCamera;

class EnemyBlock : public Collider {
public:
	EnemyBlock();
	~EnemyBlock();

	void Initialize(Model* model, const Vector3& position, const Vector3& scale, const Vector3& offset);
	void Update();
	void Draw(const ViewProjection& viewProjection);

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
	Vector3 GetTranslation() { return worldTransform_.translation_; }
	Vector3 GetWorldPosition();
	Vector3 GetRotation() { return worldTransform_.rotation_; }
	Vector3 GetScale() { return worldTransform_.scale_; }
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
	void SetPosition(const Vector3& position) { worldTransform_.translation_ = position; }
	void SetPositionX(float positionX) { worldTransform_.translation_.x = positionX; }
	void SetPositionY(float positionY) { worldTransform_.translation_.y = positionY; }
	void SetPositionZ(float positionZ) { worldTransform_.translation_.z = positionZ; }
	void SetRotation(const Vector3& rotation) { worldTransform_.rotation_ = rotation; }
	void SetRotationX(float rotationX) { worldTransform_.rotation_.x = rotationX; }
	void SetRotationY(float rotationY) { worldTransform_.rotation_.y = rotationY; }
	void SetRotationZ(float rotationZ) { worldTransform_.rotation_.z = rotationZ; }
	void SetScale(const Vector3& scale) { worldTransform_.scale_ = scale; }
	void SetScaleX(float scaleX) { worldTransform_.scale_.x = scaleX; }
	void SetScaleY(float scaleY) { worldTransform_.scale_.y = scaleY; }
	void SetScaleZ(float scaleZ) { worldTransform_.scale_.z = scaleZ; }
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

	WorldTransform worldTransform_;
	Model* model_ = nullptr;

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