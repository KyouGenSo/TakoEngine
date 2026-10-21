#include "PreviewPanel.h"

#ifdef _DEBUG

#include "PreviewViewport.h"
#include "Camera.h"
#include "imgui.h"

namespace Tako {

  PreviewPanel::PreviewPanel() = default;
  PreviewPanel::~PreviewPanel() = default;

  void PreviewPanel::Initialize(const wchar_t* debugName, const ViewportCameraSettings* cameraSettings) {
    debugName_           = debugName;
    controller_.settings = cameraSettings;
  }

  void PreviewPanel::Update() {
    if (!viewport_) {
      viewport_ = std::make_unique<PreviewViewport>();
      viewport_->Initialize(debugName_);
    }
    if (!camera_) {
      camera_ = std::make_unique<Camera>();
      camera_->SetAspect(viewport_->GetAspect());
      // 個人設定の読み込み後に初めて開くので、ここで既定の移動速度を反映する
      controller_.Reset();
    }
    controller_.ApplyTo(*camera_);
    postProcess_.RefreshLayouts();
  }

  void PreviewPanel::Finalize() {
    viewport_.reset();
    camera_.reset();
  }

  void PreviewPanel::BeginPass() {
    viewport_->BeginPass(postProcess_.HasEnabledEffects());
  }

  void PreviewPanel::EndPass() {
    viewport_->EndPass(&postProcess_, camera_.get());
  }

  void PreviewPanel::DrawCameraToolbar() {
    if (ImGui::Button("Reset Camera")) {
      controller_.Reset();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Speed: %.2f", controller_.moveSpeed);
    ImGui::TextDisabled("RMB: Look (+WASD/QE Fly, +Wheel Speed) / Alt+LMB: Orbit / MMB: Pan / Wheel: Dolly");
  }

  bool PreviewPanel::DrawImage() {
    if (!IsReady()) {
      isImageHovered_ = false;
      ImGui::TextDisabled("Initializing preview...");
      return false;
    }
    viewport_->DrawImGuiImage();
    isImageHovered_ = ImGui::IsItemHovered();
    return controller_.HandleImGuiInput();
  }

  Vector2 PreviewPanel::GetSize() const {
    return viewport_->GetSize();
  }

  bool PreviewPanel::IsReady() const {
    return viewport_ && viewport_->IsInitialized() && camera_;
  }

} // namespace Tako

#endif // _DEBUG
