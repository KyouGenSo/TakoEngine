#ifdef _DEBUG

#include "ImGuiPropertyVisitor.h"
#include "TObject.h"

#include "imgui.h"

#include <cstring>
#include <limits>
#include <numbers>

namespace Tako {

  namespace {

    constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

    float SpeedOr(const PropertyMeta& meta, float fallback)
    {
      return meta.speed > 0.0f ? meta.speed : fallback;
    }

    /// <summary>
    /// ClampMin / ClampMax を ImGui の範囲にする。どちらも無ければ 0, 0（範囲なし）
    /// </summary>
    template<class T>
    void GetRange(const PropertyMeta& meta, T& lo, T& hi)
    {
      if (!meta.min && !meta.max) {
        lo = hi = T{};
        return;
      }
      lo = meta.min ? static_cast<T>(*meta.min) : std::numeric_limits<T>::lowest();
      hi = meta.max ? static_cast<T>(*meta.max) : std::numeric_limits<T>::max();
    }

    bool DragFloats(const char* label, float* v, int count, const PropertyMeta& meta)
    {
      float lo = 0.0f;
      float hi = 0.0f;
      GetRange(meta, lo, hi);
      return ImGui::DragScalarN(label, ImGuiDataType_Float, v, count, SpeedOr(meta, 0.1f), &lo, &hi, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    }

  } // namespace

  bool ImGuiPropertyVisitor::Begin(const char* key, const PropertyMeta& meta)
  {
    if ((meta.flags & (kPropEdit | kPropVisible)) == 0) { return false; }
    if (meta.category[0] != '\0' && std::strcmp(meta.category, category_) != 0) {
      ImGui::SeparatorText(meta.category);
      category_ = meta.category;
    }
    ImGui::PushID(key);
    ImGui::BeginDisabled((meta.flags & kPropEdit) == 0);
    hasDrawn_ = true;
    return true;
  }

  void ImGuiPropertyVisitor::End(const PropertyMeta& meta, bool changed)
  {
    if (meta.tooltip[0] != '\0') {
      ImGui::SetItemTooltip("%s", meta.tooltip);
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    changed_ |= changed;
  }

  void ImGuiPropertyVisitor::Visit(const char* key, bool& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    End(meta, ImGui::Checkbox(meta.displayName, &value));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, int32_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    int32_t lo = 0;
    int32_t hi = 0;
    GetRange(meta, lo, hi);
    End(meta, ImGui::DragScalar(meta.displayName, ImGuiDataType_S32, &value, SpeedOr(meta, 1.0f), &lo, &hi, nullptr, ImGuiSliderFlags_AlwaysClamp));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, uint32_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    uint32_t lo = 0;
    uint32_t hi = 0;
    GetRange(meta, lo, hi);
    End(meta, ImGui::DragScalar(meta.displayName, ImGuiDataType_U32, &value, SpeedOr(meta, 1.0f), &lo, &hi, nullptr, ImGuiSliderFlags_AlwaysClamp));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, float& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    if ((meta.flags & kPropAngle) == 0) {
      End(meta, DragFloats(meta.displayName, &value, 1, meta));
      return;
    }
    float degrees = value * kRadToDeg;
    PropertyMeta degreeMeta = meta;
    degreeMeta.speed = SpeedOr(meta, 0.5f);
    const bool changed = DragFloats(meta.displayName, &degrees, 1, degreeMeta);
    if (changed) { value = degrees / kRadToDeg; }
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector2& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    End(meta, DragFloats(meta.displayName, &value.x, 2, meta));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector3& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const bool changed = (meta.flags & kPropColor) ? ImGui::ColorEdit3(meta.displayName, &value.x) : DragFloats(meta.displayName, &value.x, 3, meta);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector4& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const bool changed = (meta.flags & kPropColor) ? ImGui::ColorEdit4(meta.displayName, &value.x) : DragFloats(meta.displayName, &value.x, 4, meta);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, std::string& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    char buffer[256];
    strncpy_s(buffer, value.c_str(), _TRUNCATE);
    const bool changed = ImGui::InputText(meta.displayName, buffer, sizeof(buffer));
    if (changed) { value = buffer; }
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Transform& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    ImGui::SeparatorText(meta.displayName);
    bool changed = ImGui::DragFloat3("Translate", &value.translate.x, SpeedOr(meta, 0.1f));
    // 回転は度で見せ、ラジアンで持つ
    Vector3 degrees = { value.rotate.x * kRadToDeg, value.rotate.y * kRadToDeg, value.rotate.z * kRadToDeg };
    if (ImGui::DragFloat3("Rotate", &degrees.x, 0.5f)) {
      value.rotate = { degrees.x / kRadToDeg, degrees.y / kRadToDeg, degrees.z / kRadToDeg };
      changed      = true;
    }
    changed |= ImGui::DragFloat3("Scale", &value.scale.x, 0.01f);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    bool changed = false;
    if (meta.enumInfo) {
      const char* current = meta.enumInfo->FindName(value);
      if (ImGui::BeginCombo(meta.displayName, current ? current : "?")) {
        for (const EnumEntry& entry : meta.enumInfo->entries) {
          if (ImGui::Selectable(entry.name, entry.value == value)) {
            changed = entry.value != value;
            value   = entry.value;
          }
        }
        ImGui::EndCombo();
      }
    } else {
      changed = ImGui::DragScalar(meta.displayName, ImGuiDataType_S64, &value, 1.0f);
    }
    End(meta, changed);
  }

  bool DrawProperties(TObject& object)
  {
    ImGuiPropertyVisitor visitor;
    object.Reflect(visitor);
    if (!visitor.HasDrawn()) {
      ImGui::TextDisabled("No editable properties");
    }
    const bool changed = object.DrawCustomImGui() || visitor.IsChanged();
    if (changed) {
      object.PostEditChange();
    }
    return changed;
  }

} // namespace Tako

#endif // _DEBUG
