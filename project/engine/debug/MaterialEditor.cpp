#include "MaterialEditor.h"

#ifdef _DEBUG

#include <format>
#include <json.hpp>

#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Object3d.h"
#include "Renderer.h"

namespace Tako {

  using json = nlohmann::json;

  std::span<const ShortcutBinding<MaterialEditor>> MaterialEditor::Shortcuts()
  {
    static constexpr ShortcutBinding<MaterialEditor> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the material", [](MaterialEditor& editor) { editor.Save(); } },
    };
    return kShortcuts;
  }

  void MaterialEditor::Draw()
  {
    if (!*isOpen_) {
      return;
    }

    ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Material Editor", isOpen_)) {
      // UI を描く区間の前後で保存内容を比べて編集を検出する
      tracker_.BeginSection();

      ImGui::SetNextWindowSizeConstraints(ImVec2(160.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
      if (ImGui::BeginChild("List##MatEditor", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
        DrawListPane();
      }
      ImGui::EndChild();

      ImGui::SameLine();
      if (ImGui::BeginChild("Material##MatEditor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
        if (std::shared_ptr<Material> material = MaterialLibrary::GetInstance()->Find(selected_)) {
          DrawMaterialPane(*material);
          DrawTargets(material);
        }
        else {
          ImGui::TextDisabled("Select or create a material");
        }
      }
      ImGui::EndChild();

      unsavedPrompt_.Draw("Unsaved Changes##MatEditor", [this] { return Save(); });

      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
    ImGui::End();
  }

  void MaterialEditor::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
    if (tracker_.IsEdited()) {
      out.push_back(std::format("Material Editor: {}", selected_));
    }
  }

  void MaterialEditor::Select(const std::string& name)
  {
    if (!MaterialLibrary::GetInstance()->Load(name)) {
      LogToConsole(std::format("Material load failed: {}", name), DebugUIManager::LogType::Error);
      return;
    }
    selected_ = name;
    tracker_.Reset();
  }

  bool MaterialEditor::Save()
  {
    if (selected_.empty()) {
      return false;
    }
    const bool saved = MaterialLibrary::GetInstance()->Save(selected_);
    LogToConsole(std::format("Material {}: {}", saved ? "saved" : "save failed", selected_),
                 saved ? DebugUIManager::LogType::Info : DebugUIManager::LogType::Error);
    if (saved) {
      tracker_.MarkSaved();
    }
    return saved;
  }

  void MaterialEditor::DrawListPane()
  {
    ImGui::SeparatorText("Materials");
    for (const std::string& name : MaterialLibrary::GetInstance()->GetMaterialFileNames()) {
      if (ImGui::Selectable(name.c_str(), name == selected_) && name != selected_) {
        unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("Material '{}' has unsaved changes.", selected_), "Open",
                                    [this, name] { Select(name); });
      }
    }

    // 作成と同時に保存して一覧に出す
    ImGui::SeparatorText("New");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##NewName", "name", newName_.data(), newName_.size());
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##NewShader", newShader_.c_str())) {
      for (const std::string& shader : Renderer::GetInstance()->GetShaderNames()) {
        if (ImGui::Selectable(shader.c_str(), shader == newShader_)) {
          newShader_ = shader;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::BeginDisabled(newName_[0] == '\0');
    if (ImGui::Button("Create", ImVec2(-1.0f, 0.0f))) {
      const std::string name = newName_.data();
      unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("Material '{}' has unsaved changes.", selected_), "Create", [this, name] {
        if (MaterialLibrary::GetInstance()->Create(name, newShader_) && MaterialLibrary::GetInstance()->Save(name)) {
          Select(name);
        }
      });
    }
    ImGui::EndDisabled();
  }

  void MaterialEditor::DrawMaterialPane(Material& material)
  {
    MaterialLibrary* library = MaterialLibrary::GetInstance();

    ImGui::SeparatorText(tracker_.IsEdited() ? std::format("{} *", selected_).c_str() : selected_.c_str());
    if (ImGui::Button("Save")) {
      Save();
    }
    ImGui::SameLine();
    tracker_.DrawButtons();

    // シェーダーと描画設定・テクスチャのファイル名は保存内容を書き換えて当てはめる（値は名前で引き継がれる）
    json       current      = json::parse(library->Serialize(selected_));
    const auto applyCurrent = [&] { library->Apply(selected_, current.dump()); };

    if (ImGui::BeginCombo("Shader", material.GetShader()->GetName().c_str())) {
      for (const std::string& shader : Renderer::GetInstance()->GetShaderNames()) {
        if (ImGui::Selectable(shader.c_str(), shader == material.GetShader()->GetName())) {
          // 描画設定は新しいシェーダーの既定にする
          current = { { "Shader", shader }, { "Values", current["Values"] }, { "Textures", current["Textures"] } };
          applyCurrent();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::SeparatorText("Render State");
    int queue = material.GetRenderQueue();
    if (ImGui::DragInt("Queue", &queue, 1.0f, 0, 5000)) {
      material.SetRenderQueue(queue);
    }
    ImGui::SetItemTooltip("Background 1000 / Geometry 2000 / Transparent 3000 (sorted back to front) / Overlay 4000");
    RenderState state = material.GetRenderState();
    int blend = static_cast<int>(state.blend);
    if (ImGui::Combo("Blend", &blend, kBlendModeNames, IM_ARRAYSIZE(kBlendModeNames))) {
      material.SetBlendMode(static_cast<BlendMode>(blend));
    }
    int cull = static_cast<int>(state.cull);
    if (ImGui::Combo("Cull", &cull, kCullModeNames, IM_ARRAYSIZE(kCullModeNames))) {
      material.SetCullMode(static_cast<CullMode>(cull));
    }
    if (ImGui::Checkbox("Depth Test", &state.depthTest)) {
      material.SetDepthTest(state.depthTest);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Depth Write", &state.depthWrite)) {
      material.SetDepthWrite(state.depthWrite);
    }

    ImGui::SeparatorText("Properties");
    library->DrawValues(selected_);

    const std::shared_ptr<const MaterialPropertyLayout> layout = library->GetLayout(*material.GetShader());
    if (!layout->textures.empty()) {
      ImGui::SeparatorText("Textures");
      for (const std::string& slot : layout->textures) {
        std::array<char, 128> file{};
        current["Textures"].value(slot, std::string()).copy(file.data(), file.size() - 1);
        // 打ち終えたときだけ読み込む（途中の名前で TextureManager を探さない）
        ImGui::InputTextWithHint(slot.c_str(), "white.png", file.data(), file.size());
        if (ImGui::IsItemDeactivatedAfterEdit()) {
          current["Textures"][slot] = file.data();
          applyCurrent();
        }
      }
    }
  }

  void MaterialEditor::DrawTargets(const std::shared_ptr<Material>& material)
  {
    ImGui::SeparatorText("Objects");
    ImGui::TextDisabled("Applies to every mesh of the object until the object is destroyed");
    if (ImGui::BeginChild("Targets##MatEditor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
      for (Object3d* object : Object3d::GetLiveObjects()) {
        Model* model = object->GetModel();
        if (!model || model->GetMeshCount() == 0) {
          continue;
        }
        ImGui::PushID(object);
        const Vector3& position = object->GetTranslate();
        const bool     applied  = &model->GetMesh(0)->GetMaterial() == material.get();
        ImGui::Text("%s (%.1f, %.1f, %.1f)%s", object->GetModelName().empty() ? "(model)" : object->GetModelName().c_str(),
                    position.x, position.y, position.z, applied ? "  [applied]" : "");
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply")) {
          for (size_t i = 0; i < model->GetMeshCount(); ++i) {
            object->SetMaterial(i, material);
          }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
          for (size_t i = 0; i < model->GetMeshCount(); ++i) {
            object->SetMaterial(i, nullptr);
          }
        }
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
  }

} // namespace Tako

#endif // _DEBUG
