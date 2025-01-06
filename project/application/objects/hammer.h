#pragma once
#include "Model.h"
#include "Audio.h"
#include "collider.h"
#include "collisionRecord.h"
#include "collisionTypeIdDef.h"
#include "Mat4x4Func.h"
#include "Vec3Func.h"
#include "Transform.h"
#include "Object3d.h"


class Hammer final : public Collider {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Object3d* model, Object3d* effectModel);

	/// <summary>
	/// 毎フレーム処理
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// ImGui描画
	/// </summary>
	void ImGuiDraw();

	/// <summary>
	/// 衝突判定
	/// </summary>
	void OnCollision([[maybe_unused]] Collider* other) override;
	void ClearCollisionRecord() { collisionRecord_.Clear(); }

	/// <summary>
	/// Getter
	/// </summary>
	Vector3 GetCenter() const override;
	bool IsHit() const { return isHit_; }
	const Vector3& GetRotation() const { return hammerWorldTransform_.rotate; }
	const Vector3& GetTranslation() const { return hammerWorldTransform_.translate; }
	const Transform& GetWorldTransform() const { return hammerWorldTransform_; }
	const uint32_t GetSerialNumber() const { return serialNumber_; }
	const bool IsEnable() const { return enable_; }

	/// <summary>
	/// Setter
	/// </summary>
	void SetRotation(const Vector3& rotation) { hammerWorldTransform_.rotate = rotation; }
	void SetTranslation(const Vector3& translation) { hammerWorldTransform_.translate = translation; }
	void SetParent(const Transform& parent) { hammerWorldTransform_.parent_ = &parent; }
	void SetEnable(bool enable) { enable_ = enable; }

private:
	Audio* audio_ = nullptr;

	// SE
	uint32_t seHit_ = 0;

	Object3d* model_ = nullptr;
	Object3d* effectModel_ = nullptr;

	// シリアルナンバー
	uint32_t serialNumber_ = 5;

	Transform hammerWorldTransform_;
	Transform effectWorldTransform_;

	CollisionRecord collisionRecord_;

	bool isHit_ = false;
	bool enable_ = false;
};