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
    source.entry_.weight = 2.0f;
    source.entry_.values = { 1, 2 };
    source.entries_.resize(2);
    source.entries_[0].mode   = Mode::Third;
    source.entries_[1].weight = 4.0f;
    source.entries_[1].mode   = Mode::Second;
    source.entries_[1].values = { 7 };
    source.weights_ = { 5.0f };
    source.modes_   = { Mode::Third, Mode::First };
    source.target_  = { 12 };
    source.tint_    = { 0.25f, 0.5f, 0.75f, 1.0f };
    source.gradient_.keys = { { 0.0f, Color::kRed }, { 1.0f, Color::kBlue } };

    const nlohmann::json json = WriteProperties(source);
    assert(json["value"] == 4.5f);
    assert(json["isEnabled"] == false);
    assert(json["mode"] == "Third");
    assert(json["modeAsInt"] == 0);
    assert(!json.contains("transient"));
    assert(json["entry"]["weight"] == 2.0f);
    assert(json["entries"].size() == 2 && json["entries"][1]["mode"] == "Second");
    assert(json["weights"] == nlohmann::json::array({ 5.0f }));
    assert(json["modes"] == nlohmann::json::array({ "Third", "First" }));
    assert(json["target"]["actor"] == 12);
    // Color は Vector4 と同じ 4 つの数の配列
    assert(json["tint"] == nlohmann::json::array({ 0.25f, 0.5f, 0.75f, 1.0f }));
    assert(json["gradient"]["keys"].size() == 2 && json["gradient"]["keys"][1]["color"] == nlohmann::json::array({ 0.0f, 0.0f, 1.0f, 1.0f }));

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
    assert(loaded.entry_.weight == 2.0f && loaded.entry_.values == source.entry_.values);
    assert(loaded.entries_.size() == 2);
    assert(loaded.entries_[0].mode == Mode::Third && loaded.entries_[0].values.empty());
    assert(loaded.entries_[1].weight == 4.0f && loaded.entries_[1].values == source.entries_[1].values);
    assert(loaded.weights_ == source.weights_);
    assert(loaded.modes_ == source.modes_);
    assert(loaded.target_.id == 12);
    assert(loaded.tint_ == source.tint_);
    assert(loaded.gradient_.keys.size() == 2 && loaded.gradient_.keys[1].time == 1.0f && loaded.gradient_.keys[1].color == Color::kBlue);

    // キーが無ければ既定値のまま。enum は数値でも読める
    ReflectionSelfTestObject partial;
    ReadProperties(partial, { { "count", 2 }, { "mode", 0 }, { "entry", { { "values", { 3 } } } } });
    assert(partial.value_ == 1.0f);
    assert(partial.count_ == 2);
    assert(partial.mode_ == Mode::First);
    assert(partial.entry_.weight == 1.0f && partial.entry_.values == std::vector<int32_t>{ 3 });
    assert(partial.weights_.size() == 2);
    assert(partial.tint_ == Color::kWhite && partial.gradient_.keys.empty());

    assert(TypeRegistry::GetInstance()->Find("ReflectionSelfTestObject") == &ReflectionSelfTestObject::StaticType());
    assert(TypeRegistry::GetInstance()->Create("ReflectionSelfTestObject") != nullptr);
    assert(loaded.IsA<TObject>());
    assert(!TObject::StaticType().IsChildOf(ReflectionSelfTestObject::StaticType()));
  }

} // namespace Tako

#endif // _DEBUG
