#include "DebugViewport.h"

#ifdef _DEBUG
#include "EditorUI.h"

#include "EngineSettingsWindow.h"
#include "PreviewViewport.h"
#include "SceneEditor.h"
#include "Camera.h"
#include "Vector3.h"
#include "Renderer.h"
#include "SceneManager.h"
#include "DecalManager.h"
#include "GPUParticle.h"
#include "LineRenderer.h"
#include "WinApp.h"
#include "ImGuiManager.h"

#include <ImGuizmo.h>

#include <algorithm>
#include <array>

namespace Tako {

  namespace {
    constexpr float kGridHeight           = 0.01f;  // 床と同じ深さだと深度テストでちらつくため少し浮かせる
    constexpr float kGameCameraFrustumFar = 30.0f;  // far クリップ(1000)のままだと視錐台が画面を埋め尽くす
    constexpr float kMaxFocusFarRatio     = 0.5f;   // F で寄せる距離の上限（far クリップに対する割合）
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
      if (const Camera* gameCamera = *Renderer::GetInstance()->GetCamera()) {
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

    // 本編で登録したシーンの描画物を、本編と同じ順（不透明→デカール→半透明）でデバッグカメラ視点で描く。画面の UI（kQueueOverlay 以上）は描かない
    Renderer*                  renderer = Renderer::GetInstance();
    const Renderer::RenderView view     = renderer->MakeView(*camera_, viewport_->GetSize());
    renderer->RenderList(renderer->GetSceneList(), view, { .maxQueue = ShaderAsset::kQueueTransparent });
    DecalManager::GetInstance()->DrawAllForView(*camera_, viewport_->GetDepthResource(), viewport_->GetDepthSrvIndex());
    renderer->RenderList(renderer->GetSceneList(), view, { .minQueue = ShaderAsset::kQueueTransparent, .maxQueue = ShaderAsset::kQueueOverlay });
    LineRenderer* lineRenderer = LineRenderer::GetInstance();
    GPUParticle::GetInstance()->DrawForCamera(*camera_);

    const Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    lineRenderer->DrawForView(viewProjection);

    // デバッグビューだけに出すエディタ表示
    lineRenderer->BeginPreviewLines();
    if (showGrid_) {
      lineRenderer->DrawGrid(grid_->size, grid_->cellSize, kGridColor, kGridHeight);
    }
    const Camera* gameCamera = *Renderer::GetInstance()->GetCamera();
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

    // ギズモの 1 フレーム分の状態を始める（ImGui の他のウィンドウの外で呼ぶ）
    ImGuizmo::BeginFrame();

    bool inputCaptured = false;
    if (ImGui::Begin(WindowTitle::kDebugViewport, isOpen_)) {
      sceneEditor_->DrawGizmoToolbar();
      UI::ToolbarSeparator();
      UI::IconToggle(ICON_FA_BORDER_ALL "##Grid", "Grid", &showGrid_);
      ImGui::SameLine();
      UI::IconToggle(ICON_FA_VIDEO "##GameCamera", "Show the game camera", &showGameCamera_);
      ImGui::SameLine();
      if (UI::IconButton(ICON_FA_CAMERA_ROTATE, "Move this view to the game camera")) {
        if (const Camera* gameCamera = *Renderer::GetInstance()->GetCamera()) {
          cameraController_.SetFromCamera(*gameCamera);
        }
      }
      UI::ToolbarSeparator();
      cameraController_.DrawHelp();

      if (viewport_ && viewport_->IsInitialized()) {
        // ImGuizmo はカーソルの下に ImGui のアイテムがあると掴めないので、ギズモの上では画像をボタンにしない
        viewport_->DrawImGuiImage(!isGizmoHot_);
        // 画像は押した瞬間にアクティブになり、その間 IsWindowHovered は false を返す。画像そのものの上に居るかで判定する
        const bool   isImageHovered = ImGui::IsItemHovered();
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        inputCaptured = isGizmoHot_ || cameraController_.HandleImGuiInput();
        const bool wasGizmoHot = isGizmoHot_;
        isGizmoHot_ = sceneEditor_->DrawGizmo(*camera_, imageMin, imageMax);

        // 左クリックで選ぶ（ギズモを掴むクリックと、Alt + 左ドラッグの回転は除く）。F で選んだ Actor へ寄せる
        const ImGuiIO& io = ImGui::GetIO();
        if (isImageHovered) {
          if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !wasGizmoHot && !isGizmoHot_) {
            sceneEditor_->PickActor(*camera_, imageMin, imageMax);
          }
          Vector3 focus;
          float   radius = 0.0f;
          if (ImGui::IsKeyPressed(ImGuiKey_F) && !io.WantTextInput && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && sceneEditor_->GetSelectionBounds(focus, radius)) {
            cameraController_.Focus(focus, radius);
            // 地面のような大きい物は、全体が収まる距離まで引くと far クリップの外に出て何も映らなくなる
            cameraController_.distance = (std::min)(cameraController_.distance, camera_->GetFarClip() * kMaxFocusFarRatio);
          }
        }
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
