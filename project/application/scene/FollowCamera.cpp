#include "followCamera.h"
#include "imgui.h"
#ifdef DEBUG
#include "ImGuiManager.h"
#endif // DEBUG

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
  // ゲームパッドによる回転
  if (!Input::GetInstance()->RStickInDeadZone()) {
    isRotating_ = true;
  } else {
    isRotating_ = false;
  }

  if (isRotating_)
    destinationAngleY_ += Input::GetInstance()->GetRightStick().x * rotateSpeed_ /** 0.0001f*/;

  // 右スティック押し込みで角度をターゲットの後ろにリセット
  if (Input::GetInstance()->TriggerButton(XButtons.R_Thumbstick)) {
    destinationAngleY_ = target_->rotate.y;
  }

  // キーボードによる回転
  if (input_->PushKey(DIK_LEFT)) {
    destinationAngleY_ -= rotateSpeed_;
  }
  if (input_->PushKey(DIK_RIGHT)) {
    destinationAngleY_ += rotateSpeed_;
  }

  // offset
  offset_.z = Vec3::Lerp(offset_.z, offsetOrigin_.z, 0.08f);


  // カメラの角度を目標角度に向けて補間
  float angle = Vec3::LerpShortAngle(camera_->GetRotate().y, destinationAngleY_, 0.15f);
  camera_->SetRotate(Vector3(0.0f, angle, 0.0f));


  // playerの位置に補間して追従
  if (target_) {
    // ターゲットの位置に補間
    interTargetPos_ = Vec3::Lerp(interTargetPos_, target_->translate, t_);

    // ターゲットの位置にカメラを追従
    Vector3 offset = CalculateOffset();

    camera_->SetTranslate(interTargetPos_ + offset);
  }

}

void FollowCamera::Reset() {
  if (target_) {
    interTargetPos_ = target_->translate;
    camera_->SetRotate(Vector3(0.0f, target_->rotate.y, 0.0f));
  }

  destinationAngleY_ = camera_->GetRotate().y;

  offset_ = { offsetOrigin_.x, offsetOrigin_.y, offsetOrigin_.z };

  Vector3 offset = CalculateOffset();

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

  offset = Mat4x4::TransFormNormal(rotationMatrix, offset);

  return offset;
}

void FollowCamera::SetTarget(const Transform* target) {
  target_ = target;
  Reset();
}

void FollowCamera::ImGuiDraw() {
#ifdef _DEBUG
  ImGui::Begin("FollowCamera");

  ImGui::DragFloat("Offset.x", &offset_.x, 0.1f);
  ImGui::DragFloat("Offset.y", &offset_.y, 0.1f);
  ImGui::DragFloat("Offset.z", &offsetOrigin_.z, 0.1f);
  ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.01f);

  ImGui::End();
#endif _DEBUG
}