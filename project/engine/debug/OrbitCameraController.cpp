#include "OrbitCameraController.h"

#ifdef _DEBUG

#include "Camera.h"
#include "Mat4x4Func.h"
#include "ImGuiManager.h"

#include <algorithm>

namespace Tako {

  void OrbitCameraController::HandleImGuiInput()
  {
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      yaw += io.MouseDelta.x * 0.01f;
      pitch += io.MouseDelta.y * 0.01f;
      pitch = std::clamp(pitch, -1.5f, 1.5f);
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      // 世界を掴んで動かす向き: マウス右移動で注視点は左へ
      const Matrix4x4 rot = Mat4x4::MakeRotateXYZ(Vector3(pitch, yaw, 0.0f));
      const Vector3 right = Mat4x4::TransformNormal(rot, Vector3(1.0f, 0.0f, 0.0f));
      const Vector3 up = Mat4x4::TransformNormal(rot, Vector3(0.0f, 1.0f, 0.0f));
      const float panScale = distance * 0.0015f;
      target = target - right * (io.MouseDelta.x * panScale) + up * (io.MouseDelta.y * panScale);
    }
    if (io.MouseWheel != 0.0f) {
      distance = std::clamp(distance * (1.0f - io.MouseWheel * 0.1f), 0.5f, 100.0f);
    }
  }

  void OrbitCameraController::ApplyTo(Camera& camera) const
  {
    const Vector3 camRotate = { pitch, yaw, 0.0f };
    const Vector3 forward = Mat4x4::TransformNormal(Mat4x4::MakeRotateXYZ(camRotate), Vector3(0.0f, 0.0f, 1.0f));
    camera.SetRotate(camRotate);
    camera.SetTranslate(target - forward * distance);
    camera.Update();
    camera.SetViewProjectionMatrix(camera.GetViewMatrix() * camera.GetProjectionMatrix());
  }

} // namespace Tako

#endif // _DEBUG
