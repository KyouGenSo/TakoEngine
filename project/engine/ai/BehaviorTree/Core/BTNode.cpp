#include "BTNode.h"
#include "JsonPropertyVisitor.h"
#include "TypeRegistry.h"

#ifdef _DEBUG
#include "ImGuiPropertyVisitor.h"
#endif

namespace Tako {

  std::shared_ptr<BTNode> BTNode::Create(std::string_view typeName)
  {
    const TypeInfo* type = TypeRegistry::GetInstance()->Find(typeName);
    if (!type || !type->create || !type->IsChildOf(StaticType())) {
      return nullptr;
    }
    return std::shared_ptr<BTNode>(static_cast<BTNode*>(type->create().release()));
  }

  std::vector<const TypeInfo*> BTNode::GetTypes(std::string_view category)
  {
    std::vector<const TypeInfo*> types;
    for (const TypeInfo* type : TypeRegistry::GetInstance()->GetCreatableTypes(StaticType())) {
      if (category == (type->category[0] != '\0' ? type->category : "Action")) {
        types.push_back(type);
      }
    }
    return types;
  }

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
