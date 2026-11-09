#include "BTNode.h"
#include "JsonPropertyVisitor.h"

#ifdef _DEBUG
#include "ImGuiPropertyVisitor.h"
#endif

namespace Tako {

  void BTNode::ApplyParameters(const nlohmann::json& params)
  {
    ReadProperties(*this, params);
  }

  nlohmann::json BTNode::ExtractParameters() const
  {
    // Reflect は読み書き共用で const にできない。書き出しでは値を変えない
    return WriteProperties(const_cast<BTNode&>(*this));
  }

#ifdef _DEBUG
  bool BTNode::DrawImGui()
  {
    return DrawProperties(*this);
  }
#endif

} // namespace Tako
