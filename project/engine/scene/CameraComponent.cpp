#include "CameraComponent.h"
#include "Actor.h"
#include "BaseScene.h"
#include "Renderer.h"

namespace Tako {

  void CameraComponent::Draw()
  {
    const BaseScene* scene = GetOwner()->GetScene();
    if (!scene) {
      return;
    }
    for (const std::unique_ptr<Actor>& actor : scene->GetActors()) {
      for (const std::unique_ptr<Component>& component : actor->GetComponents()) {
        if (component->IsA<CameraComponent>() && static_cast<const CameraComponent*>(component.get())->priority_ > priority_) {
          return;
        }
      }
    }
    // ポインタは影・線・デカール・パーティクルも同じものを持っているので、差し替えずに値を写す
    Camera*          camera    = *Renderer::GetInstance()->GetCamera();
    const Transform  transform = GetOwner()->GetWorldTransform();
    camera->SetRotate(transform.rotate);
    camera->SetTranslate(transform.translate);
    camera->SetFovY(fovY_);
    camera->SetNearClip(nearClip_);
    camera->SetFarClip(farClip_);
  }

} // namespace Tako
