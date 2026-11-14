#include "PreviewPanel.h"

#ifdef _DEBUG

#include "PreviewViewport.h"
#include "Camera.h"
#include "EditorUI.h"
#include "Light.h"
#include "Renderer.h"

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
    if (!light_) {
      light_ = std::make_unique<Light>();
      light_->Initialize(Renderer::GetInstance()->GetDX12Basic());
      // 初期視点から見て左上の手前から照らす（色・強さ・Half-Lambert は Light の初期値）
      light_->SetDirectionalLightDirection(Vector3(0.5f, -0.8f, 0.2f).Normalize());
    }
    controller_.ApplyTo(*camera_);
    postProcess_.ResolveValues();
  }

  void PreviewPanel::Finalize() {
    viewport_.reset();
    camera_.reset();
    light_.reset();
  }

  void PreviewPanel::BeginPass() {
    viewport_->BeginPass();
  }

  void PreviewPanel::Render(const std::function<void()>& submit) {
    Renderer*            renderer = Renderer::GetInstance();
    Renderer::RenderView view     = renderer->MakeView(*camera_, GetSize());
    view.light = light_.get();

    DrawList list;
    renderer->BeginSubmit(list);
    submit();
    renderer->EndSubmit();

    const DrawFilter transparent{ .minQueue = ShaderAsset::kQueueTransparent };
    renderer->RenderList(list, view, { .maxQueue = ShaderAsset::kQueueTransparent });
    if (renderer->AnyDeclares(list, transparent, "gOpaqueTexture")) {
      viewport_->CaptureOpaqueTexture();
    }
    renderer->RenderList(list, view, transparent);
  }

  void PreviewPanel::EndPass() {
    viewport_->EndPass(&postProcess_, camera_.get());
  }

  void PreviewPanel::DrawCameraToolbar() {
    if (UI::IconButton(ICON_FA_HOUSE, "Reset camera")) {
      controller_.Reset();
    }
    ImGui::SameLine();
    controller_.DrawHelp();
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
