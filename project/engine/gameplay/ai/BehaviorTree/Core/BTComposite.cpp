#include "BTComposite.h"
#include <algorithm>

namespace Tako {

  void BTComposite::AddChild(BTNodePtr child) {
    if (child) {
      children_.push_back(child);
    }
  }

  void BTComposite::RemoveChild(BTNodePtr child) {
    children_.erase(
      std::remove(children_.begin(), children_.end(), child),
      children_.end()
    );
  }

  void BTComposite::ClearChildren() {
    children_.clear();
  }

  void BTComposite::Reset() {
    BTNode::Reset();
    currentChildIndex_ = 0;
    for (auto& child : children_) {
      child->Reset();
    }
  }

} // namespace Tako
