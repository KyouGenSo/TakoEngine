#include "ReflectionSelfTest.h"

#ifdef _DEBUG

#include "JsonPropertyVisitor.h"
#include "TypeRegistry.h"

#include <cassert>

namespace Tako {

  void RunReflectionSelfTest()
  {
    using Mode = ReflectionSelfTestObject::Mode;

    ReflectionSelfTestObject source;
    source.value_     = 4.5f;
    source.count_     = 9;
    source.isEnabled_ = false;
    source.position_  = { 4.0f, 5.0f, 6.0f };
    source.label_     = "x";
    source.mode_      = Mode::Third;
    source.modeAsInt_ = Mode::First;
    source.transient_ = 1.0f;

    const nlohmann::json json = WriteProperties(source);
    assert(json["value"] == 4.5f);
    assert(json["isEnabled"] == false);
    assert(json["mode"] == "Third");
    assert(json["modeAsInt"] == 0);
    assert(!json.contains("transient"));

    ReflectionSelfTestObject loaded;
    ReadProperties(loaded, json);
    assert(loaded.value_ == source.value_);
    assert(loaded.count_ == source.count_);
    assert(loaded.isEnabled_ == source.isEnabled_);
    assert(loaded.position_.x == 4.0f && loaded.position_.y == 5.0f && loaded.position_.z == 6.0f);
    assert(loaded.label_ == source.label_);
    assert(loaded.mode_ == source.mode_);
    assert(loaded.modeAsInt_ == source.modeAsInt_);
    assert(loaded.transient_ == 7.0f);

    // キーが無ければ既定値のまま。enum は数値でも読める
    ReflectionSelfTestObject partial;
    ReadProperties(partial, { { "count", 2 }, { "mode", 0 } });
    assert(partial.value_ == 1.0f);
    assert(partial.count_ == 2);
    assert(partial.mode_ == Mode::First);

    assert(TypeRegistry::GetInstance()->Find("ReflectionSelfTestObject") == &ReflectionSelfTestObject::StaticType());
    assert(TypeRegistry::GetInstance()->Create("ReflectionSelfTestObject") != nullptr);
    assert(loaded.IsA<TObject>());
    assert(!TObject::StaticType().IsChildOf(ReflectionSelfTestObject::StaticType()));
  }

} // namespace Tako

#endif // _DEBUG
