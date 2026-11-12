#include "SkyBoxComponent.h"
#include "SkyBox.h"

namespace Tako {

  SkyBoxComponent::SkyBoxComponent()
    : skyBox_(std::make_unique<SkyBox>())
  {
    skyBox_->Initialize("");
  }

  SkyBoxComponent::~SkyBoxComponent() = default;

  void SkyBoxComponent::Draw()
  {
    if (texture_ != appliedTexture_) {
      if (texture_.empty()) {
        hasTexture_ = false;
      }
      else if (skyBox_->SetTexture(texture_)) {
        hasTexture_ = true;
      }
      // 断られたときは前のテクスチャのまま描く
      appliedTexture_ = texture_;
    }
    if (!hasTexture_) {
      return;
    }
    skyBox_->Update();
    skyBox_->Draw();
  }

} // namespace Tako
