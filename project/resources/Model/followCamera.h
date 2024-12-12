#pragma once
#include "Input.h"
#include "Xinput.h"
#include "ViewProjection.h"
#include "WorldTransform.h"
#include "Matrix4x4Function.h"
#include "Vector3Function.h"
#include "myFunction.h"
#include <math.h>

class LockOn;

class FollowCamera {

public: // メンバ関数
	FollowCamera();
	~FollowCamera();

	void Initialize();

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
	void SetTarget(const WorldTransform* target);
	void SetLockOn(const LockOn* lockOn) { lockOn_ = lockOn; }
	void SetOffset(const Vector3& offset) { offset_ = offset; }

	/// <summary>
	/// Getters
	/// </summary>
	ViewProjection& GetViewProjection() { return viewProjection_; }
	const Vector3& GetOffset() const { return offset_; }

private: // メンバ変数
	ViewProjection viewProjection_;

	const WorldTransform* target_ = nullptr;

	Input* input_ = nullptr;

	Vector3 interTargetPos_;

	Vector3 offset_ = {0.0f, 3.0f, -13.0f};

	Vector3 offsetOrigin_ = {0.0f, 3.0f, -13.0f};

	float t_ = 0.18f;

	float destinationAngleY_ = 0.0f;

	// ロックオン
	const LockOn* lockOn_ = nullptr;

};
