#include "TObject.h"
#include "TypeRegistry.h"

namespace Tako {

  const TypeInfo& TObject::StaticType()
  {
    static const TypeInfo type{ "TObject", nullptr, nullptr };
    return type;
  }

  const TypeInfo& TObject::GetType() const
  {
    return StaticType();
  }

  void TObject::Reflect([[maybe_unused]] PropertyVisitor& v)
  {
  }

  bool TObject::IsA(const TypeInfo& type) const
  {
    return GetType().IsChildOf(type);
  }

} // namespace Tako
