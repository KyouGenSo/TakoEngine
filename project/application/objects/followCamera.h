#pragma once
#include "Input.h"
#include "Xinput.h"
#include "Mat4x4Func.h"
#include "Vec3Func.h"
#include "Transform.h"
#include "Camera.h"
#include <math.h>

class LockOn;

class FollowCamera {

public: // メンバ関数
	FollowCamera();
	~FollowCamera();

	void Initialize(Camera* camera);

	void Finalize();

	void Update();

	void Reset();

	void ResetOffset();

	void ImGuiDraw();

	// offsetの計算関数
	Vector3 CalculateOffset() const;

	// 画面揺れ
	void ShakeScreen(float power);

	/// <summary>
	/// Setters
	/// </summary>
	void SetTarget(const Transform* target);
	void SetLockOn(const LockOn* lockOn) { lockOn_ = lockOn; }
	void SetOffset(const Vector3& offset) { offset_ = offset; }

	/// <summary>
	/// Getters
	/// </summary>
	Camera* GetCamera() { return camera_; }
	const Matrix4x4& GetViewProjection() const { return camera_->GetViewProjectionMatrix(); }
	const Vector3& GetOffset() const { return offset_; }

private: // メンバ変数
	Camera* camera_;

	const Transform* target_ = nullptr;

	Input* input_ = nullptr;

	Vector3 interTargetPos_;

	Vector3 offset_ = {0.0f, 3.0f, -15.0f};

	Vector3 offsetOrigin_ = {0.0f, 3.0f, -15.0f};

	float t_ = 0.18f;

	float destinationAngleY_ = 0.0f;

	// ロックオン
	const LockOn* lockOn_ = nullptr;

	bool isRotating_ = false;

};
