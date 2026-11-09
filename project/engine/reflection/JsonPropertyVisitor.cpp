#include "JsonPropertyVisitor.h"
#include "TObject.h"

namespace Tako {

  namespace {

    bool IsSaved(const PropertyMeta& meta)
    {
      return (meta.flags & kPropTransient) == 0;
    }

    nlohmann::json ToJson(const Vector3& v)
    {
      return nlohmann::json::array({ v.x, v.y, v.z });
    }

    /// <summary>
    /// 数値が count 個の配列なら out へ読む
    /// </summary>
    bool ReadFloats(const nlohmann::json& j, float* out, size_t count)
    {
      if (!j.is_array() || j.size() != count) { return false; }
      for (size_t i = 0; i < count; ++i) {
        if (!j[i].is_number()) { return false; }
      }
      for (size_t i = 0; i < count; ++i) {
        out[i] = j[i].get<float>();
      }
      return true;
    }

    bool ReadVector3(const nlohmann::json& j, Vector3& out)
    {
      float v[3];
      if (!ReadFloats(j, v, 3)) { return false; }
      out = { v[0], v[1], v[2] };
      return true;
    }

  } // namespace

  //==================================================================
  //JsonWriteVisitor
  //==================================================================

  nlohmann::json& JsonWriteVisitor::Slot(const char* key)
  {
    nlohmann::json& top = *stack_.back();
    if (key) { return top[key]; }
    top.push_back(nullptr);
    return top.back();
  }

  bool JsonWriteVisitor::BeginStruct(const char* key, const PropertyMeta& meta)
  {
    if (!IsSaved(meta)) { return false; }
    nlohmann::json& slot = Slot(key);
    slot = nlohmann::json::object();
    stack_.push_back(&slot);
    return true;
  }

  void JsonWriteVisitor::EndStruct()
  {
    stack_.pop_back();
  }

  bool JsonWriteVisitor::BeginArray(const char* key, const PropertyMeta& meta, [[maybe_unused]] size_t& count)
  {
    if (!IsSaved(meta)) { return false; }
    nlohmann::json& slot = Slot(key);
    slot = nlohmann::json::array();
    stack_.push_back(&slot);
    return true;
  }

  void JsonWriteVisitor::EndArray()
  {
    stack_.pop_back();
  }

  void JsonWriteVisitor::Visit(const char* key, bool& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = value; }
  }

  void JsonWriteVisitor::Visit(const char* key, int32_t& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = value; }
  }

  void JsonWriteVisitor::Visit(const char* key, uint32_t& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = value; }
  }

  void JsonWriteVisitor::Visit(const char* key, float& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = value; }
  }

  void JsonWriteVisitor::Visit(const char* key, Vector2& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = nlohmann::json::array({ value.x, value.y }); }
  }

  void JsonWriteVisitor::Visit(const char* key, Vector3& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = ToJson(value); }
  }

  void JsonWriteVisitor::Visit(const char* key, Vector4& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = nlohmann::json::array({ value.x, value.y, value.z, value.w }); }
  }

  void JsonWriteVisitor::Visit(const char* key, std::string& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) { Slot(key) = value; }
  }

  void JsonWriteVisitor::Visit(const char* key, Transform& value, const PropertyMeta& meta)
  {
    if (IsSaved(meta)) {
      Slot(key) = { { "scale", ToJson(value.scale) }, { "rotate", ToJson(value.rotate) }, { "translate", ToJson(value.translate) } };
    }
  }

  void JsonWriteVisitor::VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta)
  {
    if (!IsSaved(meta)) { return; }
    // 名前の無い値は数値で残す（TENUM に無い値でも失わないため）
    const char* name = meta.enumInfo && (meta.flags & kPropEnumAsInt) == 0 ? meta.enumInfo->FindName(value) : nullptr;
    if (name) {
      Slot(key) = name;
    } else {
      Slot(key) = value;
    }
  }

  //==================================================================
  //JsonReadVisitor
  //==================================================================

  const nlohmann::json* JsonReadVisitor::Find(const char* key, const PropertyMeta& meta)
  {
    if (!IsSaved(meta)) { return nullptr; }
    Frame& top = stack_.back();
    if (!key) {
      if (!top.node->is_array() || top.nextIndex >= top.node->size()) { return nullptr; }
      return &(*top.node)[top.nextIndex++];
    }
    if (!top.node->is_object()) { return nullptr; }
    auto it = top.node->find(key);
    return it != top.node->end() ? &*it : nullptr;
  }

  bool JsonReadVisitor::BeginStruct(const char* key, const PropertyMeta& meta)
  {
    const nlohmann::json* j = Find(key, meta);
    if (!j || !j->is_object()) { return false; }
    stack_.push_back({ j, 0 });
    return true;
  }

  void JsonReadVisitor::EndStruct()
  {
    stack_.pop_back();
  }

  bool JsonReadVisitor::BeginArray(const char* key, const PropertyMeta& meta, size_t& count)
  {
    const nlohmann::json* j = Find(key, meta);
    if (!j || !j->is_array()) { return false; }
    count = j->size();
    stack_.push_back({ j, 0 });
    return true;
  }

  void JsonReadVisitor::EndArray()
  {
    stack_.pop_back();
  }

  void JsonReadVisitor::Visit(const char* key, bool& value, const PropertyMeta& meta)
  {
    if (const nlohmann::json* j = Find(key, meta); j && j->is_boolean()) { value = j->get<bool>(); }
  }

  void JsonReadVisitor::Visit(const char* key, int32_t& value, const PropertyMeta& meta)
  {
    if (const nlohmann::json* j = Find(key, meta); j && j->is_number()) { value = j->get<int32_t>(); }
  }

  void JsonReadVisitor::Visit(const char* key, uint32_t& value, const PropertyMeta& meta)
  {
    if (const nlohmann::json* j = Find(key, meta); j && j->is_number()) { value = j->get<uint32_t>(); }
  }

  void JsonReadVisitor::Visit(const char* key, float& value, const PropertyMeta& meta)
  {
    if (const nlohmann::json* j = Find(key, meta); j && j->is_number()) { value = j->get<float>(); }
  }

  void JsonReadVisitor::Visit(const char* key, Vector2& value, const PropertyMeta& meta)
  {
    float v[2];
    if (const nlohmann::json* j = Find(key, meta); j && ReadFloats(*j, v, 2)) { value = { v[0], v[1] }; }
  }

  void JsonReadVisitor::Visit(const char* key, Vector3& value, const PropertyMeta& meta)
  {
    if (const nlohmann::json* j = Find(key, meta)) { ReadVector3(*j, value); }
  }

  void JsonReadVisitor::Visit(const char* key, Vector4& value, const PropertyMeta& meta)
  {
    float v[4];
    if (const nlohmann::json* j = Find(key, meta); j && ReadFloats(*j, v, 4)) { value = { v[0], v[1], v[2], v[3] }; }
  }

  void JsonReadVisitor::Visit(const char* key, std::string& value, const PropertyMeta& meta)
  {
    if (const nlohmann::json* j = Find(key, meta); j && j->is_string()) { value = j->get<std::string>(); }
  }

  void JsonReadVisitor::Visit(const char* key, Transform& value, const PropertyMeta& meta)
  {
    const nlohmann::json* j = Find(key, meta);
    if (!j || !j->is_object()) { return; }
    if (auto it = j->find("scale"); it != j->end()) { ReadVector3(*it, value.scale); }
    if (auto it = j->find("rotate"); it != j->end()) { ReadVector3(*it, value.rotate); }
    if (auto it = j->find("translate"); it != j->end()) { ReadVector3(*it, value.translate); }
  }

  void JsonReadVisitor::VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta)
  {
    const nlohmann::json* j = Find(key, meta);
    if (!j) { return; }
    if (j->is_number()) {
      value = j->get<int64_t>();
    } else if (j->is_string() && meta.enumInfo) {
      if (std::optional<int64_t> found = meta.enumInfo->FindValue(j->get<std::string>())) { value = *found; }
    }
  }

  //==================================================================
  //補助関数
  //==================================================================

  nlohmann::json WriteProperties(TObject& object)
  {
    nlohmann::json out;
    JsonWriteVisitor visitor(out);
    object.Reflect(visitor);
    object.PostSave(out);
    return out;
  }

  void ReadProperties(TObject& object, const nlohmann::json& in)
  {
    JsonReadVisitor visitor(in);
    object.Reflect(visitor);
    object.PostLoad(in);
  }

} // namespace Tako
