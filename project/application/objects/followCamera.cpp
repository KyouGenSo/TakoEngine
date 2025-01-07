#include "followCamera.h"
#include "ImGuiManager.h"
#include "lockOn.h"
#include "ImGuiManager.h"

FollowCamera::FollowCamera() {}

FollowCamera::~FollowCamera() {}

void FollowCamera::Initialize(Camera* camera) {

	input_ = Input::GetInstance();

	camera_ = camera;
}

void FollowCamera::Finalize()
{

}

void FollowCamera::Update() {

	float rotateSpeed = 0.03f;

	if (lockOn_->isTargetExist()) {
		// ロックオン中はロックオン対象に向く
		Vector3 lockOnPos = lockOn_->GetTargetPos();
		Vector3 sub = lockOnPos - target_->translate;
		float angle = std::atan2(sub.x, sub.z);
		destinationAngleY_ = angle;

		// offset
		offset_.z = Vec3::Lerp(offset_.z, -14.0f, 0.1f);
	}
	else {
		// ゲームパッドによる回転
		XINPUT_STATE joyState;
		if (input_->GetJoystickState(0, joyState)) {

			if (joyState.Gamepad.sThumbRX / 32767.0f > 0.3f || joyState.Gamepad.sThumbRX / 32767.0f < -0.3f) {
				isRotating_ = true;
			} else {
				isRotating_ = false;
			}

			if(isRotating_)
			destinationAngleY_ += (float)joyState.Gamepad.sThumbRX * rotateSpeed * 0.0001f;

			// 右スティック押し込みで角度をターゲットの後ろにリセット
			if (joyState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) {
				destinationAngleY_ = target_->rotate.y;
			}
		}

		// キーボードによる回転
		if (input_->PushKey(DIK_LEFT)) {
			destinationAngleY_ -= rotateSpeed;		
		}
		if (input_->PushKey(DIK_RIGHT)) {
			destinationAngleY_ += rotateSpeed;
		}

		// offset
		offset_.z = Vec3::Lerp(offset_.z, offsetOrigin_.z, 0.08f);
	}

	// カメラの角度を目標角度に向けて補間
	float angle = Vec3::LerpShortAngle(camera_->GetRotate().y, destinationAngleY_, 0.15f);
	camera_->SetRotate(Vector3(0.0f, angle, 0.0f));


	// playerの位置に補間して追従
	if (target_) {
		// ターゲットの位置に補間
		interTargetPos_ = Vec3::Lerp(interTargetPos_, target_->translate, t_);

		// ターゲットの位置にカメラを追従
		Vector3 offset = CalculateOffset();

		//m_camera_.translation_ = interTargetPos_ + offset;
		camera_->SetTranslate(interTargetPos_ + offset);

		// カメラのビュー行列を更新
		//m_camera_.UpdateMatrix();
	}

}

void FollowCamera::Reset() { 
	if (target_) {
		interTargetPos_ = target_->translate;
		//m_camera_.rotation_.y = target_->rotate.y;
		camera_->SetRotate(Vector3(0.0f, target_->rotate.y, 0.0f));
	}

	//destinationAngleY_ = m_camera_.rotation_.y;
	destinationAngleY_ = camera_->GetRotate().y;

	offset_ = { offsetOrigin_.x, offsetOrigin_.y, offsetOrigin_.z };

	Vector3 offset = CalculateOffset();

	//m_camera_.translation_ = interTargetPos_ + offset;
	camera_->SetTranslate(interTargetPos_ + offset);
	
}

void FollowCamera::ResetOffset() { 
	offset_.x = offsetOrigin_.x;
	offset_.y = offsetOrigin_.y;
}

Vector3 FollowCamera::CalculateOffset() const {
	Vector3 offset = offset_;

	// カメラの角度から回転行列を算出
	Matrix4x4 rotationMatrix = Mat4x4::MakeRotateXYZ(camera_->GetRotate());

	offset = Mat4x4::TransFormNormal(offset, rotationMatrix);

	return offset;
}

void FollowCamera::ShakeScreen(float power) { 
	float randomX = Vec3::Rand(-power, power);
	float randomY = Vec3::Rand(-power, power);

	offset_.x += randomX;
	offset_.y += randomY;
}

void FollowCamera::SetTarget(const Transform* target) { 
	target_ = target;
	Reset();
}

void FollowCamera::ImGuiDraw() {
#ifdef _DEBUG
	ImGui::Begin("Camera");

	//ImGui::DragFloat3("Position", &m_camera_.translation_.x, 0.1f);
	//ImGui::DragFloat3("Rotation", &m_camera_.rotation_.x, 0.1f);


	ImGui::End();
#endif _DEBUG
}