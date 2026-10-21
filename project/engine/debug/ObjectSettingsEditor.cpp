#include "ObjectSettingsEditor.h"

#ifdef _DEBUG

#include <algorithm>
#include <format>
#include <map>

#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "MaterialLibrary.h"
#include "Object3d.h"

namespace Tako {

  namespace {

    /// <summary>
    /// 「上書きする」のチェックと入力欄の 1 行。チェックを外した間は、コードの値を灰色で出す
    /// </summary>
    /// <param name="codeValue">上書きしないときの値（チェックを入れたときの初期値にもする）</param>
    /// <param name="widget">bool(const char* label, T& value)。値を変えたら true</param>
    /// <returns>設定を変えたら true</returns>
    template<class T, class Widget>
    bool OverrideField(const char* label, std::optional<T>& value, const T& codeValue, Widget&& widget)
    {
      ImGui::PushID(label);
      bool changed = false;
      bool enabled = value.has_value();
      if (ImGui::Checkbox("##Override", &enabled)) {
        value   = enabled ? std::optional<T>(codeValue) : std::nullopt;
        changed = true;
      }
      ImGui::SetItemTooltip("Override the value set in code");
      ImGui::SameLine();
      ImGui::BeginDisabled(!enabled);
      T shown = value.value_or(codeValue);
      if (widget(label, shown) && enabled) {
        value   = shown;
        changed = true;
      }
      ImGui::EndDisabled();
      ImGui::PopID();
      return changed;
    }

  } // anonymous namespace

  std::span<const ShortcutBinding<ObjectSettingsEditor>> ObjectSettingsEditor::Shortcuts()
  {
    static constexpr ShortcutBinding<ObjectSettingsEditor> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the object settings", [](ObjectSettingsEditor& editor) { editor.Save(); } },
    };
    return kShortcuts;
  }

  void ObjectSettingsEditor::Draw()
  {
    if (!*isOpen_) {
      return;
    }

    ImGui::SetNextWindowSize(ImVec2(720.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Object Settings", isOpen_)) {
      // UI を描く区間の前後で保存内容を比べて編集を検出する
      tracker_.BeginSection();

      if (ImGui::Button("Save")) {
        Save();
      }
      ImGui::SameLine();
      tracker_.DrawButtons();
      ImGui::SameLine();
      ImGui::TextDisabled(tracker_.IsEdited() ? "ObjectSettings.json *" : "ObjectSettings.json");

      ImGui::SetNextWindowSizeConstraints(ImVec2(160.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
      if (ImGui::BeginChild("List##ObjectSettings", ImVec2(240.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
        DrawListPane();
      }
      ImGui::EndChild();

      ImGui::SameLine();
      if (ImGui::BeginChild("Settings##ObjectSettings", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
        DrawSettingsPane();
      }
      ImGui::EndChild();

      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
    ImGui::End();
  }

  void ObjectSettingsEditor::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
    if (tracker_.IsEdited()) {
      out.push_back("Object Settings");
    }
  }

  bool ObjectSettingsEditor::Save()
  {
    const bool saved = ObjectSettings::GetInstance()->Save();
    LogToConsole(saved ? "Object settings saved" : "Object settings save failed", saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
    if (saved) {
      tracker_.MarkSaved();
    }
    return saved;
  }

  void ObjectSettingsEditor::DrawListPane()
  {
    // 名前ごと・名前の無いものはモデルごとに数える
    std::map<std::string, int> named;
    std::map<std::string, int> unnamed;
    for (const std::string& name : ObjectSettings::GetInstance()->GetNames()) {
      named[name];
    }
    for (const Object3d* object : Object3d::GetLiveObjects()) {
      if (!object->GetName().empty()) {
        ++named[object->GetName()];
      }
      else {
        ++unnamed[object->GetModelName().empty() ? "(model)" : object->GetModelName()];
      }
    }

    ImGui::SeparatorText("Names");
    for (const auto& [name, count] : named) {
      if (ImGui::Selectable(std::format("{}  ({})###{}", name, count, name).c_str(), name == selected_)) {
        selected_ = name;
      }
    }

    ImGui::SeparatorText("Add");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##NewName", "name passed to SetName", newName_.data(), newName_.size());
    ImGui::BeginDisabled(newName_[0] == '\0');
    if (ImGui::Button("Add", ImVec2(-1.0f, 0.0f))) {
      selected_ = newName_.data();
      ObjectSettings::GetInstance()->Edit(selected_);
      ObjectSettings::GetInstance()->MarkChanged();
      newName_.fill('\0');
    }
    ImGui::EndDisabled();

    if (!unnamed.empty()) {
      ImGui::SeparatorText("Unnamed");
      ImGui::TextDisabled("(?)");
      ImGui::SetItemTooltip("Call Object3d::SetName in game code to save settings for these objects");
      for (const auto& [model, count] : unnamed) {
        ImGui::TextDisabled("%s  (%d)", model.c_str(), count);
      }
    }
  }

  void ObjectSettingsEditor::DrawSettingsPane()
  {
    if (selected_.empty()) {
      ImGui::TextDisabled("Select a name");
      return;
    }

    // 上書きしない項目に出すコードの値は、同じ名前の最初の Object3d から取る（上書きを外している間は Object3d の値がコードの値）
    const auto&     objects = Object3d::GetLiveObjects();
    const auto      found   = std::ranges::find_if(objects, [&](const Object3d* object) { return object->GetName() == selected_; });
    const Object3d* object  = found != objects.end() ? *found : nullptr;
    if (!object) {
      ImGui::TextDisabled("No live object has this name. The values in gray are the Object3d defaults");
    }

    ObjectSettings*  settings  = ObjectSettings::GetInstance();
    ObjectOverrides& overrides = settings->Edit(selected_);
    bool             changed   = false;

    ImGui::SeparatorText(selected_.c_str());

    const std::vector<std::string> materials = MaterialLibrary::GetInstance()->GetMaterialFileNames();
    changed |= OverrideField<std::string>("Material", overrides.material, materials.empty() ? std::string() : materials.front(), [&](const char* label, std::string& value) {
      bool picked = false;
      if (ImGui::BeginCombo(label, overrides.material ? value.c_str() : "(mesh material)")) {
        for (const std::string& material : materials) {
          if (ImGui::Selectable(material.c_str(), material == value)) {
            value  = material;
            picked = true;
          }
        }
        ImGui::EndCombo();
      }
      return picked;
    });
    ImGui::SetItemTooltip("Shared by every object with this name. SetMaterialColor / SetTexture in code do not change it");

    changed |= OverrideField<uint32_t>("Mask ID", overrides.maskId, object ? object->GetMaskId() : 0u, [](const char* label, uint32_t& value) {
      int id = static_cast<int>(value);
      if (!ImGui::InputInt(label, &id)) {
        return false;
      }
      value = static_cast<uint32_t>(std::clamp(id, 0, 255));
      return true;
    });
    ImGui::SetItemTooltip("0 = none. Post effects with the same Mask ID apply only to this object");

    changed |= OverrideField<int32_t>("Sorting Order", overrides.sortingOrder, object ? object->GetSortingOrder() : 0, [](const char* label, int32_t& value) {
      return ImGui::DragInt(label, &value, 0.2f);
    });
    ImGui::SetItemTooltip("Draw order within the same render queue (smaller first)");

    changed |= OverrideField<bool>("Cast Shadow", overrides.castShadow, object ? object->CastsShadow() : true, [](const char* label, bool& value) {
      return ImGui::Checkbox(label, &value);
    });

    changed |= OverrideField<bool>("Transparent", overrides.transparent, object ? object->IsTransparent() : false, [](const char* label, bool& value) {
      return ImGui::Checkbox(label, &value);
    });
    ImGui::SetItemTooltip("Double-sided, no depth write, drawn in the transparent queue (back to front)");

    if (changed) {
      settings->MarkChanged();
    }
  }

} // namespace Tako

#endif // _DEBUG
