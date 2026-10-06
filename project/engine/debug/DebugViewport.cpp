#include "DebugViewport.h"

#ifdef _DEBUG

#include "EngineSettingsWindow.h"
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
    constexpr float kGridHeight           = 0.01f;  // 床と同じ深さだと深度テストでちらつくため少し浮かせる
    constexpr float kGameCameraFrustumFar = 30.0f;  // far クリップ(1000)のままだと視錐台が画面を埋め尽くす
    const Vector4   kGridColor            = { 0.35f, 0.35f, 0.35f, 1.0f };
    const Vector4   kGameCameraColor      = { 1.0f, 0.85f, 0.2f, 1.0f };
  }

  DebugViewport::DebugViewport() = default;
  DebugViewport::~DebugViewport() = default;

  void DebugViewport::Update()
  {
    if (!*isOpen_) {
      Finalize();
      return;
    }

    if (!viewport_) {
      viewport_ = std::make_unique<PreviewViewport>();
      viewport_->Initialize(L"DebugViewport", static_cast<uint32_t>(WinApp::clientWidth), static_cast<uint32_t>(WinApp::clientHeight));
    }

    if (!camera_) {
      camera_ = std::make_unique<Camera>();
      camera_->SetAspect(viewport_->GetAspect());
      // 個人設定の読み込み後に初めて開くので、ここで既定の移動速度を反映する
      cameraController_.Reset();
      // 開いた直後はゲームカメラと同じ位置・向きから始める
      if (const Camera* gameCamera = *Object3dBasic::GetInstance()->GetCamera()) {
        cameraController_.SetFromCamera(*gameCamera);
      }
    }

    cameraController_.ApplyTo(*camera_);
  }

  void DebugViewport::DrawPass()
  {
    if (!*isOpen_ || !viewport_ || !viewport_->IsInitialized() || !camera_) {
      return;
    }

    viewport_->BeginPass();

    // シーンの 3D 描画をデバッグカメラ視点で再実行する。再実行中に積まれる線分（コライダー等）は本編分と重複するので捨てる
    LineRenderer* lineRenderer = LineRenderer::GetInstance();
    Object3dBasic::GetInstance()->SetView(*camera_);
    const uint32_t lineMark = lineRenderer->GetBatchMark();
    SceneManager::GetInstance()->Draw();
    lineRenderer->RollbackBatch(lineMark);

    DecalManager::GetInstance()->DrawAllForView(*camera_, viewport_->GetDepthResource(), viewport_->GetDepthSrvIndex());
    GPUParticle::GetInstance()->DrawForCamera(*camera_);

    const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    lineRenderer->DrawForView(viewProjection);

    // デバッグビューだけに出すエディタ表示
    lineRenderer->BeginPreviewLines();
    if (showGrid_) {
      lineRenderer->DrawGrid(grid_->size, grid_->cellSize, kGridColor, kGridHeight);
    }
    const Camera* gameCamera = *Object3dBasic::GetInstance()->GetCamera();
    if (showGameCamera_ && gameCamera) {
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

    viewport_->EndPass();
  }

  bool DebugViewport::Draw()
  {
    if (!*isOpen_) {
      return false;
    }

    bool inputCaptured = false;
    if (ImGui::Begin("Debug Viewport", isOpen_)) {
      if (ImGui::Button("Sync to Game Camera##DbgView")) {
        if (const Camera* gameCamera = *Object3dBasic::GetInstance()->GetCamera()) {
          cameraController_.SetFromCamera(*gameCamera);
        }
      }
      ImGui::SameLine();
      ImGui::Checkbox("Grid##DbgView", &showGrid_);
      ImGui::SameLine();
      ImGui::Checkbox("Game Camera##DbgView", &showGameCamera_);
      ImGui::SameLine();
      ImGui::TextDisabled("Speed: %.2f", cameraController_.moveSpeed);
      ImGui::TextDisabled("RMB: Look (+WASD/QE Fly, +Wheel Speed) / Alt+LMB: Orbit / MMB: Pan / Wheel: Dolly");

      if (viewport_ && viewport_->IsInitialized()) {
        viewport_->DrawImGuiImage();
        inputCaptured = cameraController_.HandleImGuiInput();
      }
      else {
        ImGui::TextDisabled("Initializing viewport...");
      }
    }
    ImGui::End();
    return inputCaptured;
  }

  void DebugViewport::Finalize()
  {
    viewport_.reset();
    camera_.reset();
  }

} // namespace Tako

#endif // _DEBUG
