#include "ObjectSettingsEditor.h"

#ifdef _DEBUG

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <map>
#include <optional>

#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "Material.h"
#include "MaterialLibrary.h"
#include "Model.h"
#include "Object3d.h"
#include "OverrideField.h"
#include "Renderer.h"

namespace Tako {

  namespace {

    using json = nlohmann::json;

    constexpr size_t kNoMesh = (std::numeric_limits<size_t>::max)();

    // 値の並びと、上書きしないときの値
    struct MaterialSource {
      std::shared_ptr<const MaterialPropertyLayout> layout;
      json                                          values = json::object();
    };

    /// <summary>
    /// コードで上書き中なら、行の横に印を出す（その欄の値はコードで外すまで効かない）
    /// </summary>
    void DrawCodeMark(bool overridden, const char* setter)
    {
      if (!overridden) {
        return;
      }
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "code");
      ImGui::SetItemTooltip("Overridden in code by Object3d::%s.\nThis value has no effect until the code removes it (std::nullopt)", setter);
    }

    /// <summary>
    /// マテリアルがあればそのシェーダー、無ければモデルの既定のマテリアル（生存中のものが無ければ Lit の 0）から値の並びと値を取る
    /// </summary>
    MaterialSource FindSource(const std::string& materialName, const Object3d* object, size_t meshIndex)
    {
      MaterialLibrary* library = MaterialLibrary::GetInstance();
      if (!materialName.empty()) {
        // MarkChanged で読み込み済み（Load は読めないと毎フレーム記録を出すので使わない）
        const std::shared_ptr<Material> material = library->Find(materialName);
        if (!material) {
          return {};
        }
        std::shared_ptr<const MaterialPropertyLayout> layout = library->GetLayout(*material->GetShader());
        json values = layout->SerializeValues(material->GetConstantBytes());
        return { std::move(layout), std::move(values) };
      }
      if (const Mesh* mesh = object && object->GetModel() ? object->GetModel()->GetMesh(meshIndex) : nullptr) {
        const Material& base = mesh->GetMaterial();
        std::shared_ptr<const MaterialPropertyLayout> layout = library->GetLayout(*base.GetShader());
        json values = layout->SerializeValues(base.GetConstantBytes());
        return { std::move(layout), std::move(values) };
      }
      const ShaderAsset* lit = Renderer::GetInstance()->FindShader("Lit");
      return lit ? MaterialSource{ .layout = library->GetLayout(*lit) } : MaterialSource{};
    }

    /// <summary>
    /// Material・Values・Textures の上書き欄
    /// </summary>
    /// <param name="parent">メッシュの欄なら全メッシュ用の設定（引き継ぐ値に使う）。全メッシュ用なら nullptr</param>
    /// <param name="meshIndex">引き継ぐ値を読むメッシュの番号（kNoMesh なら読まない）</param>
    /// <returns>変えたら true</returns>
    bool DrawMaterialOverrides(MaterialOverrides& overrides, const MaterialOverrides* parent, const Object3d* object, size_t meshIndex, const std::vector<std::string>& materials)
    {
      bool changed = false;
      const Object3d::CodeOverrides* code = object ? &object->GetCodeOverrides() : nullptr;

      const std::string inheritedMaterial = parent && parent->material ? *parent->material : std::string();
      changed |= OverrideField<std::string>("Material", overrides.material, inheritedMaterial, [&](const char* label, std::string& value) {
        bool picked = false;
        if (ImGui::BeginCombo(label, value.empty() ? "(model default)" : value.c_str())) {
          if (ImGui::Selectable("(model default)", value.empty())) {
            value.clear();
            picked = true;
          }
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
      ImGui::SetItemTooltip("(model default): the material made from the model file.\n"
                            "Values / Textures are laid over the drawn material's values of the same name when drawing (the material itself is not changed)");
      // メッシュの欄はそのメッシュ、全メッシュ用の欄はどれかのメッシュをコードで差し替えていれば印を出す
      const bool codeMaterial = code && (parent ? meshIndex < code->materials.size() && code->materials[meshIndex]
                                                : std::ranges::any_of(code->materials, [](const auto& material) { return material != nullptr; }));
      DrawCodeMark(codeMaterial, "SetMaterial");

      const std::string materialName = overrides.material.value_or(inheritedMaterial);
      if (parent && meshIndex != kNoMesh) {
        ImGui::TextDisabled("Drawn with: %s", codeMaterial ? "(code SetMaterial)" : materialName.empty() ? "(model default)" : std::format("{} (Object Settings)", materialName).c_str());
      }
      MaterialSource    source       = FindSource(materialName, object, meshIndex);
      if (!source.layout) {
        ImGui::TextDisabled("Material not found");
        return changed;
      }
      if (parent) {
        source.values.update(parent->values);
      }

      for (const MaterialProperty& property : source.layout->properties) {
        if (property.type == MaterialProperty::Type::Unsupported) {
          continue;
        }
        const auto          found = overrides.values.find(property.name);
        std::optional<json> value = found != overrides.values.end() ? std::optional<json>(*found) : std::nullopt;
        const json          inherited = source.values.value(property.name, json());
        if (OverrideField<json>(property.name.c_str(), value, inherited, [&](const char*, json& edited) { return source.layout->DrawValueImGui(property, edited); })) {
          if (value) {
            overrides.values[property.name] = *value;
          }
          else {
            overrides.values.erase(property.name);
          }
          changed = true;
        }
        DrawCodeMark(code && code->properties.Has(property.name), "SetMaterialColor / GetMaterialProperties");
      }

      for (const std::string& slot : source.layout->textures) {
        const auto                 found = overrides.textures.find(slot);
        std::optional<std::string> file  = found != overrides.textures.end() ? std::optional<std::string>(found->second) : std::nullopt;
        std::string                inherited;
        if (parent) {
          if (const auto parentFile = parent->textures.find(slot); parentFile != parent->textures.end()) {
            inherited = parentFile->second;
          }
        }
        const bool edited = OverrideField<std::string>(slot.c_str(), file, inherited, [](const char* label, std::string& value) {
          std::array<char, 128> buffer{};
          value.copy(buffer.data(), buffer.size() - 1);
          // 打ち終えたときか、アセットウィンドウから落とされたときだけ読み込む（途中の名前で TextureManager を探さない）
          ImGui::InputTextWithHint(label, "(inherited)", buffer.data(), buffer.size());
          const std::optional<std::string> dropped = AcceptTextureDrop();
          if (ImGui::IsItemDeactivatedAfterEdit() || dropped) {
            value = dropped.value_or(buffer.data());
            return true;
          }
          return false;
        });
        if (edited) {
          if (file) {
            overrides.textures[slot] = *file;
          }
          else {
            overrides.textures.erase(slot);
          }
          changed = true;
        }
        DrawCodeMark(code && code->properties.Has(slot), "SetTexture / GetMaterialProperties");
      }
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

    // 灰色の値は既定（エンジンの初期値・マテリアル・モデル）。コードの上書きは同じ名前の最初の Object3d から読み、印で出す
    const auto&                    objects  = Object3d::GetLiveObjects();
    const auto                     found    = std::ranges::find_if(objects, [&](const Object3d* object) { return object->GetName() == selected_; });
    const Object3d*                object   = found != objects.end() ? *found : nullptr;
    const Object3d::CodeOverrides* code     = object ? &object->GetCodeOverrides() : nullptr;
    const DrawItem                 defaults{};
    ImGui::TextDisabled("Priority: code (Object3d::Set...) > these settings > default (gray)");
    if (!object) {
      ImGui::TextDisabled("No live object has this name");
    }

    ObjectSettings*  settings  = ObjectSettings::GetInstance();
    ObjectOverrides& overrides = settings->Edit(selected_);
    bool             changed   = false;

    ImGui::SeparatorText(selected_.c_str());

    const auto checkbox = [](const char* label, bool& value) { return ImGui::Checkbox(label, &value); };
    changed |= OverrideField<bool>("Visible", overrides.visible, true, checkbox);
    ImGui::SetItemTooltip("Unchecked hides the object (also from shadows, render passes and the object mask)");

    changed |= OverrideField<uint32_t>("Mask ID", overrides.maskId, defaults.maskId, [](const char* label, uint32_t& value) {
      int id = static_cast<int>(value);
      if (!ImGui::InputInt(label, &id)) {
        return false;
      }
      value = static_cast<uint32_t>(std::clamp(id, 0, 255));
      return true;
    });
    ImGui::SetItemTooltip("0 = none. Post effects with the same Mask ID apply only to this object");
    DrawCodeMark(code && code->maskId, "SetMaskId");

    changed |= OverrideField<int32_t>("Sorting Order", overrides.sortingOrder, defaults.sortingOrder, [](const char* label, int32_t& value) {
      return ImGui::DragInt(label, &value, 0.2f);
    });
    ImGui::SetItemTooltip("Draw order within the same render queue (smaller first)");
    DrawCodeMark(code && code->sortingOrder, "SetSortingOrder");

    changed |= OverrideField<bool>("Cast Shadow", overrides.castShadow, defaults.castShadow, checkbox);
    DrawCodeMark(code && code->castShadow, "SetCastShadow");

    changed |= OverrideField<bool>("Transparent", overrides.transparent, defaults.transparent, checkbox);
    ImGui::SetItemTooltip("Double-sided, no depth write, drawn in the transparent queue (back to front)");
    DrawCodeMark(code && code->transparent, "SetTransparent");

    const std::vector<std::string> materials = MaterialLibrary::GetInstance()->GetMaterialFileNames();
    ImGui::SeparatorText("Material (all meshes)");
    changed |= DrawMaterialOverrides(overrides, nullptr, object, object ? 0 : kNoMesh, materials);

    // 生存中のオブジェクトのメッシュ（モデルの順）と、設定だけにあるメッシュ名
    ImGui::SeparatorText("Meshes");
    std::vector<std::string> meshNames = object ? object->GetMeshNames() : std::vector<std::string>{};
    const size_t             liveCount = meshNames.size();
    for (const auto& [meshName, mesh] : overrides.meshes) {
      if (std::ranges::find(meshNames, meshName) == meshNames.end()) {
        meshNames.push_back(meshName);
      }
    }
    if (meshNames.empty()) {
      ImGui::TextDisabled("No live object has this name");
    }
    for (size_t i = 0; i < meshNames.size(); ++i) {
      const std::string& meshName = meshNames[i];
      if (!ImGui::TreeNode(std::format("{}##Mesh{}", meshName.empty() ? "(unnamed)" : meshName, i).c_str())) {
        continue;
      }
      MeshOverrides& mesh = overrides.meshes[meshName];
      changed |= OverrideField<bool>("Visible", mesh.visible, i < liveCount ? object->GetModel()->IsMeshVisible(meshName) : true, checkbox);
      DrawCodeMark(code && code->meshVisible.contains(meshName), "SetMeshVisible");
      changed |= DrawMaterialOverrides(mesh, &overrides, object, i < liveCount ? i : kNoMesh, materials);
      ImGui::TreePop();
    }

    if (changed) {
      settings->MarkChanged();
    }
  }

} // namespace Tako

#endif // _DEBUG
