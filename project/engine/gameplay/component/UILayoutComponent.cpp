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
    EnsureLoaded();
    // 読んでいなければ名前が無い
    if (instance_->GetName().empty() || !visible_) {
      return;
    }
    instance_->SetSortingOrder(sortingOrder_);
    instance_->Update();
    instance_->Draw();
  }

  UILayout& UILayoutComponent::GetLayout()
  {
    EnsureLoaded();
    return *instance_;
  }

  void UILayoutComponent::EnsureLoaded()
  {
    if (layout_ == loadedLayout_) {
      return;
    }
    instance_ = std::make_unique<UILayout>();
    // 無いファイルを Load すると空のレイアウトを作って記録を出すので、先に確かめる
    if (!layout_.empty() && std::filesystem::is_regular_file(std::string(UILayout::kDirectory) + layout_ + ".json")) {
      instance_->Load(layout_);
    }
    loadedLayout_ = layout_;
  }

} // namespace Tako
