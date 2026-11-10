#include "UILayoutComponent.h"
#include "UILayout.h"

#include <filesystem>

namespace Tako {

  UILayoutComponent::UILayoutComponent()
    : instance_(std::make_unique<UILayout>())
  {
  }

  UILayoutComponent::~UILayoutComponent() = default;

  void UILayoutComponent::Draw()
  {
    if (layout_ != loadedLayout_) {
      instance_ = std::make_unique<UILayout>();
      // 無いファイルを Load すると空のレイアウトを作って記録を出すので、先に確かめる
      if (!layout_.empty() && std::filesystem::is_regular_file(std::string(UILayout::kDirectory) + layout_ + ".json")) {
        instance_->Load(layout_);
      }
      loadedLayout_ = layout_;
    }
    // 読んでいなければ名前が無い
    if (instance_->GetName().empty()) {
      return;
    }
    instance_->Update();
    instance_->Draw();
  }

} // namespace Tako
