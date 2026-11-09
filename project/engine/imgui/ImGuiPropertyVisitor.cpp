#ifdef _DEBUG

#include "ImGuiPropertyVisitor.h"
#include "TObject.h"

#include "imgui.h"

#include <cstdio>
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
    if (key) {
      if (meta.category[0] != '\0' && std::strcmp(meta.category, category_) != 0) {
        ImGui::SeparatorText(meta.category);
        category_ = meta.category;
      }
      ImGui::PushID(key);
      label_ = meta.displayName;
    } else {
      const int index = indices_.back()++;
      ImGui::PushID(index);
      snprintf(indexLabel_, sizeof(indexLabel_), "[%d]", index);
      label_ = indexLabel_;
    }
    ImGui::BeginDisabled((meta.flags & kPropEdit) == 0);
    hasDrawn_ = true;
    return true;
  }

  void ImGuiPropertyVisitor::EnterNested()
  {
    nestedCategories_.push_back(category_);
    category_ = "";
  }

  void ImGuiPropertyVisitor::LeaveNested()
  {
    category_ = nestedCategories_.back();
    nestedCategories_.pop_back();
    ImGui::TreePop();
    ImGui::EndDisabled();
    ImGui::PopID();
  }

  bool ImGuiPropertyVisitor::BeginStruct(const char* key, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return false; }
    // 名前のある構造体は開いておき、配列の要素は閉じておく（要素が多いと縦に長くなるため）
    const bool isOpen = ImGui::TreeNodeEx(label_, key ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
    if (!isOpen) {
      End(meta, false);
      return false;
    }
    if (meta.tooltip[0] != '\0') {
      ImGui::SetItemTooltip("%s", meta.tooltip);
    }
    EnterNested();
    return true;
  }

  void ImGuiPropertyVisitor::EndStruct()
  {
    LeaveNested();
  }

  bool ImGuiPropertyVisitor::BeginArray(const char* key, const PropertyMeta& meta, size_t& count)
  {
    if (!Begin(key, meta)) { return false; }
    // 個数が変わっても開閉の状態が変わらないよう、ID は ### の後ろで固定する
    char header[160];
    snprintf(header, sizeof(header), "%s (%zu)###Elements", label_, count);
    const bool isOpen = ImGui::TreeNodeEx(header, key ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
    if (meta.tooltip[0] != '\0') {
      ImGui::SetItemTooltip("%s", meta.tooltip);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) {
      ++count;
      changed_ = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(count == 0);
    if (ImGui::SmallButton("-")) {
      --count;
      changed_ = true;
    }
    ImGui::EndDisabled();
    if (!isOpen) {
      ImGui::EndDisabled();
      ImGui::PopID();
      return false;
    }
    indices_.push_back(0);
    EnterNested();
    return true;
  }

  void ImGuiPropertyVisitor::EndArray()
  {
    indices_.pop_back();
    LeaveNested();
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
    End(meta, ImGui::Checkbox(label_, &value));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, int32_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    int32_t lo = 0;
    int32_t hi = 0;
    GetRange(meta, lo, hi);
    End(meta, ImGui::DragScalar(label_, ImGuiDataType_S32, &value, SpeedOr(meta, 1.0f), &lo, &hi, nullptr, ImGuiSliderFlags_AlwaysClamp));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, uint32_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    uint32_t lo = 0;
    uint32_t hi = 0;
    GetRange(meta, lo, hi);
    End(meta, ImGui::DragScalar(label_, ImGuiDataType_U32, &value, SpeedOr(meta, 1.0f), &lo, &hi, nullptr, ImGuiSliderFlags_AlwaysClamp));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, float& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    if ((meta.flags & kPropAngle) == 0) {
      End(meta, DragFloats(label_, &value, 1, meta));
      return;
    }
    float degrees = value * kRadToDeg;
    PropertyMeta degreeMeta = meta;
    degreeMeta.speed = SpeedOr(meta, 0.5f);
    const bool changed = DragFloats(label_, &degrees, 1, degreeMeta);
    if (changed) { value = degrees / kRadToDeg; }
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector2& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    End(meta, DragFloats(label_, &value.x, 2, meta));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector3& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const bool changed = (meta.flags & kPropColor) ? ImGui::ColorEdit3(label_, &value.x) : DragFloats(label_, &value.x, 3, meta);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector4& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const bool changed = (meta.flags & kPropColor) ? ImGui::ColorEdit4(label_, &value.x) : DragFloats(label_, &value.x, 4, meta);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, std::string& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    char buffer[256];
    strncpy_s(buffer, value.c_str(), _TRUNCATE);
    const bool changed = ImGui::InputText(label_, buffer, sizeof(buffer));
    if (changed) { value = buffer; }
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Transform& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    ImGui::SeparatorText(label_);
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
      if (ImGui::BeginCombo(label_, current ? current : "?")) {
        for (const EnumEntry& entry : meta.enumInfo->entries) {
          if (ImGui::Selectable(entry.name, entry.value == value)) {
            changed = entry.value != value;
            value   = entry.value;
          }
        }
        ImGui::EndCombo();
      }
    } else {
      changed = ImGui::DragScalar(label_, ImGuiDataType_S64, &value, 1.0f);
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
