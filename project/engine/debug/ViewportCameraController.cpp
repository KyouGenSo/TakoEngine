#include "ViewportCameraController.h"

#ifdef _DEBUG

#include "Camera.h"
#include "Mat4x4Func.h"
#include "ImGuiManager.h"

#include <algorithm>
#include <cmath>

namespace Tako {

  namespace {
    constexpr float kRotateSpeed   = 0.01f;    // rad/px
    constexpr float kPitchLimit    = 1.5f;
    constexpr float kPanPerPixel   = 0.0015f;  // moveSpeed 1 あたりの 1px のパン量
    constexpr float kWheelStep     = 0.1f;     // moveSpeed 1 あたりの 1 ノッチの前後量
    constexpr float kSpeedStep     = 1.25f;    // 右押し中ホイール 1 ノッチの速度倍率
    constexpr float kMinMoveSpeed  = 0.05f;
    constexpr float kMaxMoveSpeed  = 500.0f;
    constexpr float kMinDistance   = 0.5f;     // 回転中心がカメラ背後に回らないための下限
    constexpr float kFocusDistance = 4.0f;

    Vector3 RotateDirection(float pitch, float yaw, const Vector3& local)
    {
      return Mat4x4::TransformNormal(Mat4x4::MakeRotateXYZ(Vector3(pitch, yaw, 0.0f)), local);
    }
  }

  bool ViewportCameraController::HandleImGuiInput()
  {
    const ImGuiIO& io = ImGui::GetIO();
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    const bool looking = active && ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const float rotateSpeed = kRotateSpeed * (settings ? settings->lookSensitivity : 1.0f);

    if (looking) {
      // カメラ位置を固定したまま向きだけ変え、注視点を付け直す
      const Vector3 position = target - Forward() * distance;
      yaw += io.MouseDelta.x * rotateSpeed;
      pitch = std::clamp(pitch + io.MouseDelta.y * rotateSpeed, -kPitchLimit, kPitchLimit);
      target = position + Forward() * distance;

      Vector3 move = { 0.0f, 0.0f, 0.0f };
      if (ImGui::IsKeyDown(ImGuiKey_W)) { move = move + Forward(); }
      if (ImGui::IsKeyDown(ImGuiKey_S)) { move = move - Forward(); }
      if (ImGui::IsKeyDown(ImGuiKey_D)) { move = move + Right(); }
      if (ImGui::IsKeyDown(ImGuiKey_A)) { move = move - Right(); }
      if (ImGui::IsKeyDown(ImGuiKey_E)) { move = move + Vector3(0.0f, 1.0f, 0.0f); }
      if (ImGui::IsKeyDown(ImGuiKey_Q)) { move = move - Vector3(0.0f, 1.0f, 0.0f); }
      target = target + move * (moveSpeed * io.DeltaTime);
    }
    else if (active && io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      yaw += io.MouseDelta.x * rotateSpeed;
      pitch = std::clamp(pitch + io.MouseDelta.y * rotateSpeed, -kPitchLimit, kPitchLimit);
    }

    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      // 世界を掴んで動かす向き: マウス右移動で注視点は左へ
      const float panScale = moveSpeed * kPanPerPixel;
      target = target - Right() * (io.MouseDelta.x * panScale) + Up() * (io.MouseDelta.y * panScale);
    }

    if ((hovered || active) && io.MouseWheel != 0.0f) {
      if (looking) {
        moveSpeed = std::clamp(moveSpeed * std::pow(kSpeedStep, io.MouseWheel), kMinMoveSpeed, kMaxMoveSpeed);
      }
      else {
        distance -= io.MouseWheel * moveSpeed * kWheelStep;
        if (distance < kMinDistance) {
          // 注視点を追い越す分は注視点ごと前進させる
          target = target + Forward() * (kMinDistance - distance);
          distance = kMinDistance;
        }
      }
    }

    return hovered || active;
  }

  void ViewportCameraController::ApplyTo(Camera& camera) const
  {
    if (settings) {
      camera.SetFovY(settings->fovY);
    }
    camera.SetRotate(Vector3(pitch, yaw, 0.0f));
    camera.SetTranslate(target - Forward() * distance);
    camera.Update();
    camera.SetViewProjectionMatrix(camera.GetViewMatrix() * camera.GetProjectionMatrix());
  }

  void ViewportCameraController::Reset()
  {
    *this = ViewportCameraController{ .settings = settings };
    if (settings) {
      moveSpeed = settings->moveSpeed;
    }
  }

  void ViewportCameraController::Focus(const Vector3& point)
  {
    target = point;
    distance = kFocusDistance;
  }

  void ViewportCameraController::SetFromCamera(const Camera& camera)
  {
    const Vector3 rotate = camera.GetRotate();
    yaw = rotate.y;
    pitch = std::clamp(rotate.x, -kPitchLimit, kPitchLimit);
    target = camera.GetTranslate() + Forward() * distance;
  }

  Vector3 ViewportCameraController::Forward() const
  {
    return RotateDirection(pitch, yaw, Vector3(0.0f, 0.0f, 1.0f));
  }

  Vector3 ViewportCameraController::Right() const
  {
    return RotateDirection(pitch, yaw, Vector3(1.0f, 0.0f, 0.0f));
  }

  Vector3 ViewportCameraController::Up() const
  {
    return RotateDirection(pitch, yaw, Vector3(0.0f, 1.0f, 0.0f));
  }

} // namespace Tako

#endif // _DEBUG
