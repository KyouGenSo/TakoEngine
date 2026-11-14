#ifdef _DEBUG

#include "ImGuiPropertyVisitor.h"
#include "EditorUI.h"
#include "Actor.h"
#include "AssetBrowser.h"
#include "BaseScene.h"
#include "Gradient.h"
#include "SceneEditor.h"
#include "SceneManager.h"
#include "TObject.h"
#include "TypeRegistry.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
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
      return UI::DragScalarN(label, ImGuiDataType_Float, v, count, SpeedOr(meta, 0.1f), &lo, &hi, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    }

    /// <summary>
    /// Required の欄が空なら、枠を赤くする
    /// </summary>
    /// <returns>true なら、欄を描いた後に PopStyleColor を呼ぶ</returns>
    bool PushRequiredColor(const PropertyMeta& meta, bool isEmpty)
    {
      if (!isEmpty || (meta.flags & kPropRequired) == 0) { return false; }
      const ImVec4& error = GetEditorColors().error;
      ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(error.x, error.y, error.z, 0.35f));
      return true;
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

  void ImGuiPropertyVisitor::PreviewGradient(const Gradient& gradient)
  {
    UI::Label("Preview");
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max = { min.x + ImGui::CalcItemWidth(), min.y + ImGui::GetFrameHeight() };
    ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));

    // 透明な所が分かるよう、ColorButton と同じ市松模様を下に敷く
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::RenderColorRectWithAlphaCheckerboard(drawList, min, max, 0, (max.y - min.y) * 0.5f, ImVec2(0.0f, 0.0f));
    // 1 px ごとに Evaluate の色で塗る（同じ位置に重ねた段階の、色の切り替わりもそのまま出る）
    for (float x = min.x; x < max.x; x += 1.0f) {
      const Color color = gradient.Evaluate((x - min.x + 0.5f) / (max.x - min.x));
      drawList->AddRectFilled(ImVec2(x, min.y), ImVec2((std::min)(x + 1.0f, max.x), max.y), ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a)));
    }
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
    if (ImGui::SmallButton(ICON_FA_PLUS)) {
      ++count;
      changed_ = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(count == 0);
    if (ImGui::SmallButton(ICON_FA_MINUS)) {
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
    End(meta, UI::Checkbox(label_, &value));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, int32_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    int32_t lo = 0;
    int32_t hi = 0;
    GetRange(meta, lo, hi);
    End(meta, UI::DragScalar(label_, ImGuiDataType_S32, &value, SpeedOr(meta, 1.0f), &lo, &hi, nullptr, ImGuiSliderFlags_AlwaysClamp));
  }

  void ImGuiPropertyVisitor::Visit(const char* key, uint32_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    uint32_t lo = 0;
    uint32_t hi = 0;
    GetRange(meta, lo, hi);
    End(meta, UI::DragScalar(label_, ImGuiDataType_U32, &value, SpeedOr(meta, 1.0f), &lo, &hi, nullptr, ImGuiSliderFlags_AlwaysClamp));
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
    const bool changed = (meta.flags & kPropColor) ? UI::ColorEdit3(label_, &value.x) : DragFloats(label_, &value.x, 3, meta);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Vector4& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const bool changed = (meta.flags & kPropColor) ? UI::ColorEdit4(label_, &value.x) : DragFloats(label_, &value.x, 4, meta);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, std::string& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const bool isMarked = PushRequiredColor(meta, value.empty());
    bool       changed  = false;
    if (meta.asset[0] != '\0') {
      changed = DrawAsset(value, meta);
    } else {
      char buffer[256];
      strncpy_s(buffer, value.c_str(), _TRUNCATE);
      changed = UI::InputText(label_, buffer, sizeof(buffer));
      if (changed) { value = buffer; }
    }
    if (isMarked) { ImGui::PopStyleColor(); }
    End(meta, changed);
  }

  bool ImGuiPropertyVisitor::DrawAsset(std::string& value, const PropertyMeta& meta)
  {
    std::optional<std::string> selected;
    UI::Label(label_);
    // 右に Open のボタンを置く分だけ狭める
    ImGui::SetNextItemWidth(-(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x));
    if (ImGui::BeginCombo(UI::HiddenLabel(label_), value.empty() ? "(none)" : value.c_str(), ImGuiComboFlags_HeightLarge)) {
      if (ImGui::Selectable("(none)", value.empty())) {
        selected = std::string();
      }
      for (const std::string& candidate : ListAssets(meta.asset)) {
        if (ImGui::Selectable(candidate.c_str(), candidate == value)) {
          selected = candidate;
        }
      }
      ImGui::EndCombo();
    }
    else {
      // 一覧を開いている間は直前のアイテムが一覧の中のものになるので、閉じているときだけ受け付ける
      ImGui::SetItemTooltip("%s", meta.tooltip[0] != '\0' ? meta.tooltip : "Pick from the list or drop a file from the Assets window");
      if (std::optional<std::string> dropped = AcceptAssetDrop(meta.asset)) {
        selected = std::move(dropped);
      }
    }
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::BeginDisabled(value.empty());
    if (UI::IconButton(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE, "Open in its editor")) {
      OpenAsset(meta.asset, value);
    }
    ImGui::EndDisabled();
    if (!selected || *selected == value) {
      return false;
    }
    value = std::move(*selected);
    return true;
  }

  void ImGuiPropertyVisitor::Visit(const char* key, Transform& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    ImGui::SeparatorText(label_);
    bool changed = UI::DragFloat3("Translate", &value.translate.x, SpeedOr(meta, 0.1f));
    // 回転は度で見せ、ラジアンで持つ
    Vector3 degrees = { value.rotate.x * kRadToDeg, value.rotate.y * kRadToDeg, value.rotate.z * kRadToDeg };
    if (UI::DragFloat3("Rotate", &degrees.x, 0.5f)) {
      value.rotate = { degrees.x / kRadToDeg, degrees.y / kRadToDeg, degrees.z / kRadToDeg };
      changed      = true;
    }
    changed |= UI::DragFloat3("Scale", &value.scale.x, 0.01f);
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::Visit(const char* key, ActorRef& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    const BaseScene* scene  = SceneManager::GetInstance()->GetCurrentScene();
    const Actor*     target = scene && value.id != 0 ? scene->FindActor(value.id) : nullptr;
    const std::string preview = target ? target->GetName() : value.id == 0 ? std::string("(none)") : std::format("Missing (id {})", value.id);

    // 型名が見つからなければ絞らない
    const TypeInfo* wanted   = meta.actorWith[0] != '\0' ? TypeRegistry::GetInstance()->Find(meta.actorWith) : nullptr;
    const bool      isMarked = PushRequiredColor(meta, target == nullptr);

    ActorId selected = value.id;
    if (UI::BeginCombo(label_, preview.c_str(), ImGuiComboFlags_HeightLarge)) {
      if (ImGui::Selectable("(none)", value.id == 0)) {
        selected = 0;
      }
      if (scene) {
        for (const std::unique_ptr<Actor>& actor : scene->GetActors()) {
          if (wanted && !actor->Has(*wanted)) { continue; }
          ImGui::PushID(static_cast<int>(actor->GetId()));
          if (ImGui::Selectable(actor->GetName().c_str(), actor->GetId() == value.id)) {
            selected = actor->GetId();
          }
          ImGui::PopID();
        }
      }
      ImGui::EndCombo();
    }
    else {
      // 一覧を開いている間は直前のアイテムが一覧の中のものになるので、閉じているときだけ受け付ける
      if (meta.tooltip[0] == '\0') {
        ImGui::SetItemTooltip("Pick from the list or drop an actor from the Scene Hierarchy");
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kActorPayload)) {
          const ActorId dropped = *static_cast<const ActorId*>(payload->Data);
          const Actor*  actor   = scene ? scene->FindActor(dropped) : nullptr;
          if (!wanted || (actor && actor->Has(*wanted))) {
            selected = dropped;
          }
        }
        ImGui::EndDragDropTarget();
      }
    }
    if (isMarked) { ImGui::PopStyleColor(); }
    const bool changed = selected != value.id;
    value.id = selected;
    End(meta, changed);
  }

  void ImGuiPropertyVisitor::VisitEnum(const char* key, int64_t& value, const PropertyMeta& meta)
  {
    if (!Begin(key, meta)) { return; }
    bool changed = false;
    if (meta.enumInfo) {
      const char* current = meta.enumInfo->FindName(value);
      if (UI::BeginCombo(label_, current ? current : "?")) {
        for (const EnumEntry& entry : meta.enumInfo->entries) {
          if (ImGui::Selectable(entry.name, entry.value == value)) {
            changed = entry.value != value;
            value   = entry.value;
          }
        }
        ImGui::EndCombo();
      }
    } else {
      changed = UI::DragScalar(label_, ImGuiDataType_S64, &value, 1.0f);
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
