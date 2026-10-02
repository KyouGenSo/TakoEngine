#include "DebugUIManager.h"

#ifdef _DEBUG

#include "PreviewViewport.h"
#include "Camera.h"
#include "Object3dBasic.h"
#include "SceneManager.h"
#include "DecalManager.h"
#include "GPUParticle.h"
#include "LineRenderer.h"
#include "WinApp.h"
#include "ImGuiManager.h"

#include <array>

namespace Tako {

  namespace {
    constexpr float kGridSize             = 200.0f;
    constexpr float kGridSubdivision      = 100.0f;
    constexpr float kGridHeight           = 0.01f;  // 床と同じ深さだと深度テストでちらつくため少し浮かせる
    constexpr float kGameCameraFrustumFar = 30.0f;  // far クリップ(1000)のままだと視錐台が画面を埋め尽くす
    const Vector4   kGridColor            = { 0.35f, 0.35f, 0.35f, 1.0f };
    const Vector4   kGameCameraColor      = { 1.0f, 0.85f, 0.2f, 1.0f };
  }

  void DebugUIManager::UpdateDebugViewport()
  {
    if (!windowVisibility_["DebugViewport"]) {
      FinalizeDebugViewport();
      return;
    }

    if (!debugViewport_) {
      debugViewport_ = std::make_unique<PreviewViewport>();
      debugViewport_->Initialize(L"DebugViewport", static_cast<uint32_t>(WinApp::clientWidth), static_cast<uint32_t>(WinApp::clientHeight));
    }

    if (!debugViewCamera_) {
      debugViewCamera_ = std::make_unique<Camera>();
      debugViewCamera_->SetAspect(debugViewport_->GetAspect());
      // 開いた直後はゲームカメラと同じ位置・向きから始める
      if (const Camera* gameCamera = *Object3dBasic::GetInstance()->GetCamera()) {
        debugCameraController_.SetFromCamera(*gameCamera);
      }
    }

    debugCameraController_.ApplyTo(*debugViewCamera_);
  }

  void DebugUIManager::DrawDebugViewportPass()
  {
    if (!windowVisibility_["DebugViewport"] || !debugViewport_ || !debugViewport_->IsInitialized() || !debugViewCamera_) {
      return;
    }

    debugViewport_->BeginPass();

    // シーンの 3D 描画をデバッグカメラ視点で再実行する。再実行中に積まれる線分（コライダー等）は本編分と重複するので捨てる
    LineRenderer* lineRenderer = LineRenderer::GetInstance();
    Object3dBasic::GetInstance()->SetView(*debugViewCamera_);
    const uint32_t lineMark = lineRenderer->GetBatchMark();
    SceneManager::GetInstance()->Draw();
    lineRenderer->RollbackBatch(lineMark);

    DecalManager::GetInstance()->DrawAllForView(*debugViewCamera_, debugViewport_->GetDepthResource(), debugViewport_->GetDepthSrvIndex());
    GPUParticle::GetInstance()->DrawForCamera(*debugViewCamera_);

    const Matrix4x4& viewProjection = debugViewCamera_->GetViewProjectionMatrix();
    lineRenderer->DrawForView(viewProjection);

    // デバッグビューだけに出すエディタ表示
    lineRenderer->BeginPreviewLines();
    if (debugViewShowGrid_) {
      lineRenderer->DrawGrid(kGridSize, kGridSubdivision, kGridColor, kGridHeight);
    }
    const Camera* gameCamera = *Object3dBasic::GetInstance()->GetCamera();
    if (debugViewShowGameCamera_ && gameCamera) {
      // near 面・far 面・それらを結ぶ側面の 12 辺
      const std::array<Vector3, 8> corners = gameCamera->GetFrustumCornersWithCustomFar(kGameCameraFrustumFar);
      for (int i = 0; i < 4; ++i) {
        const int next = (i + 1) % 4;
        lineRenderer->DrawLine(corners[i], corners[next], kGameCameraColor);
        lineRenderer->DrawLine(corners[i + 4], corners[next + 4], kGameCameraColor);
        lineRenderer->DrawLine(corners[i], corners[i + 4], kGameCameraColor);
      }
    }
    lineRenderer->EndPreviewLines();
    lineRenderer->DrawPreviewLines(viewProjection);

    debugViewport_->EndPass();
  }

  void DebugUIManager::DrawDebugViewportWindow()
  {
    if (ImGui::Begin("Debug Viewport", &windowVisibility_["DebugViewport"])) {
      if (ImGui::Button("Sync to Game Camera##DbgView")) {
        if (const Camera* gameCamera = *Object3dBasic::GetInstance()->GetCamera()) {
          debugCameraController_.SetFromCamera(*gameCamera);
        }
      }
      ImGui::SameLine();
      ImGui::Checkbox("Grid##DbgView", &debugViewShowGrid_);
      ImGui::SameLine();
      ImGui::Checkbox("Game Camera##DbgView", &debugViewShowGameCamera_);
      ImGui::SameLine();
      ImGui::TextDisabled("Speed: %.2f", debugCameraController_.moveSpeed);
      ImGui::TextDisabled("RMB: Look (+WASD/QE Fly, +Wheel Speed) / Alt+LMB: Orbit / MMB: Pan / Wheel: Dolly");

      if (debugViewport_ && debugViewport_->IsInitialized()) {
        debugViewport_->DrawImGuiImage();
        isPreviewInputCaptured_ |= debugCameraController_.HandleImGuiInput();
      }
      else {
        ImGui::TextDisabled("Initializing viewport...");
      }
    }
    ImGui::End();
  }

  void DebugUIManager::FinalizeDebugViewport()
  {
    debugViewport_.reset();
    debugViewCamera_.reset();
  }

} // namespace Tako

#endif // _DEBUG
