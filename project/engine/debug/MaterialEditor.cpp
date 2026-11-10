#include "MaterialEditor.h"

#ifdef _DEBUG

#include <format>
#include <json.hpp>

#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "Material.h"
#include "OverrideField.h"
#include "PostProcess.h"
#include "Renderer.h"

namespace Tako {

  using json = nlohmann::json;

  namespace {

    /// <summary>
    /// 名前の表から列挙の値を選ぶ。offset は「値 - 添字」（D3D12_COMPARISON_FUNC は 1 から始まる）
    /// </summary>
    template<class T, size_t N>
    bool EnumCombo(const char* label, T& value, const char* const (&names)[N], int offset = 0)
    {
      int index = static_cast<int>(value) - offset;
      if (!ImGui::Combo(label, &index, names, static_cast<int>(N))) {
        return false;
      }
      value = static_cast<T>(index + offset);
      return true;
    }

  } // anonymous namespace

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
        }
        else if (MaterialLibrary::GetInstance()->IsPostEffectMaterial(selected_)) {
          DrawPostEffectPane();
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
    // 描画物のマテリアルでも LoadPostEffect で読み込みは済む（戻り値が nullptr になるだけ）
    MaterialLibrary* library = MaterialLibrary::GetInstance();
    if (!library->LoadPostEffect(name) && !library->Load(name)) {
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

  void MaterialEditor::RequestOpen(const std::string& name)
  {
    if (name == selected_) {
      return;
    }
    unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("Material '{}' has unsaved changes.", selected_), "Open",
                                [this, name] { Select(name); });
  }

  void MaterialEditor::DrawListPane()
  {
    ImGui::SeparatorText("Materials");
    for (const std::string& name : MaterialLibrary::GetInstance()->GetMaterialFileNames()) {
      const std::string label = MaterialLibrary::GetInstance()->IsPostEffectMaterial(name) ? std::format("{}  (PostEffect)##{}", name, name) : name;
      if (ImGui::Selectable(label.c_str(), name == selected_)) {
        RequestOpen(name);
      }
    }

    // 作成と同時に保存して一覧に出す
    ImGui::SeparatorText("New");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##NewName", "name", newName_.data(), newName_.size());
    ImGui::SetNextItemWidth(-1.0f);
    // 用途はシェーダーで決まる（UE の Material Domain と同じ）
    if (ImGui::BeginCombo("##NewShader", newShader_.c_str())) {
      for (const std::string& shader : Renderer::GetInstance()->GetShaderNames()) {
        if (ImGui::Selectable(shader.c_str(), shader == newShader_)) {
          newShader_ = shader;
        }
      }
      ImGui::SeparatorText("PostEffect");
      for (const std::string& shader : MaterialLibrary::GetInstance()->GetPostEffectShaderNames()) {
        if (ImGui::Selectable(std::format("{}##post", shader).c_str(), shader == newShader_)) {
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
    ImGui::TextDisabled("Assign to objects in Tools > Object Settings");

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

    // チェックを入れた項目だけシェーダーの値を上書きする（外した項目はシェーダーの変更に追従する）
    ImGui::SeparatorText("Render State");
    ImGui::TextDisabled("Unchecked items use the shader's value (Shader Editor)");
    const ShaderAsset&          shader    = *material.GetShader();
    const GraphicsPipelineDesc* forward   = shader.GetPass(ShaderPass::Forward);
    const RenderState           inherited = forward ? forward->renderState : RenderState{};
    RenderStateOverride         state     = material.GetStateOverride();
    std::optional<int32_t>      queue     = material.GetQueueOverride();
    bool                        changed   = false;

    changed |= OverrideField<int32_t>("Queue", queue, shader.GetDefaultQueue(), [](const char* label, int32_t& value) {
      return ImGui::DragInt(label, &value, 1.0f, 0, 5000);
    });
    ImGui::SetItemTooltip("Background 1000 / Geometry 2000 / Transparent 3000 (sorted back to front) / Overlay 4000");
    changed |= OverrideField<BlendMode>("Blend", state.blend, inherited.blend, [](const char* label, BlendMode& value) {
      return EnumCombo(label, value, kBlendModeNames);
    });
    changed |= OverrideField<CullMode>("Cull", state.cull, inherited.cull, [](const char* label, CullMode& value) {
      return EnumCombo(label, value, kCullModeNames);
    });
    changed |= OverrideField<bool>("Depth Test", state.depthTest, inherited.depthTest, [](const char* label, bool& value) {
      return ImGui::Checkbox(label, &value);
    });
    changed |= OverrideField<bool>("Depth Write", state.depthWrite, inherited.depthWrite, [](const char* label, bool& value) {
      return ImGui::Checkbox(label, &value);
    });
    changed |= OverrideField<D3D12_COMPARISON_FUNC>("Depth Func", state.depthFunc, inherited.depthFunc, [](const char* label, D3D12_COMPARISON_FUNC& value) {
      return EnumCombo(label, value, kDepthFuncNames, 1);
    });
    ImGui::SetItemTooltip("Greater draws only where something else is in front (x-ray)");
    if (changed) {
      material.SetStateOverride(state);
      material.SetQueueOverride(queue);
    }

    ImGui::SeparatorText("Properties");
    library->DrawValues(selected_);

    const std::shared_ptr<const MaterialPropertyLayout> layout = library->GetLayout(*material.GetShader());
    if (!layout->textures.empty()) {
      ImGui::SeparatorText("Textures");
      for (const std::string& slot : layout->textures) {
        std::array<char, 128> file{};
        current["Textures"].value(slot, std::string()).copy(file.data(), file.size() - 1);
        // 打ち終えたときか、アセットウィンドウから落とされたときだけ読み込む（途中の名前で TextureManager を探さない）
        ImGui::InputTextWithHint(slot.c_str(), "white.png", file.data(), file.size());
        const std::optional<std::string> dropped = AcceptAssetDrop("Texture");
        if (ImGui::IsItemDeactivatedAfterEdit() || dropped) {
          current["Textures"][slot] = dropped.value_or(file.data());
          applyCurrent();
        }
      }
    }
  }

  void MaterialEditor::DrawPostEffectPane()
  {
    MaterialLibrary* library = MaterialLibrary::GetInstance();

    ImGui::SeparatorText(tracker_.IsEdited() ? std::format("{} *", selected_).c_str() : selected_.c_str());
    if (ImGui::Button("Save")) {
      Save();
    }
    ImGui::SameLine();
    tracker_.DrawButtons();
    ImGui::TextDisabled("Add to the stack in PostEffect Settings (Materials). Edits apply to every effect that uses this material");

    // シェーダーとテクスチャのファイル名は保存内容を書き換えて当てはめる（値は名前で引き継がれる）
    json              current      = json::parse(library->Serialize(selected_));
    const std::string shader       = current.value("Shader", std::string());
    const auto        applyCurrent = [&] { library->Apply(selected_, current.dump()); };

    // 用途の違うシェーダーへは替えられない（描画物のマテリアルは新しく作る）
    if (ImGui::BeginCombo("Shader", shader.c_str())) {
      for (const std::string& name : library->GetPostEffectShaderNames()) {
        if (ImGui::Selectable(name.c_str(), name == shader)) {
          current["Shader"] = name;
          applyCurrent();
        }
      }
      ImGui::EndCombo();
    }

    const PostEffectDefinition* definition = FindPostEffect(shader);
    const PropertyBlock*        defaults   = definition ? std::get_if<PropertyBlock>(&definition->defaultParam) : nullptr;
    if (!defaults) {
      ImGui::TextDisabled("Shader not registered (check the error in Shader Editor)");
      return;
    }

    ImGui::SeparatorText("Properties");
    library->DrawValues(selected_);

    if (!defaults->layout->textures.empty()) {
      ImGui::SeparatorText("Textures");
      for (const std::string& slot : defaults->layout->textures) {
        std::array<char, 128> file{};
        current["Textures"].value(slot, std::string()).copy(file.data(), file.size() - 1);
        // 空なら .shader.json の Textures のまま描く
        ImGui::InputTextWithHint(slot.c_str(), "(shader's texture)", file.data(), file.size());
        const std::optional<std::string> dropped = AcceptAssetDrop("Texture");
        if (ImGui::IsItemDeactivatedAfterEdit() || dropped) {
          current["Textures"][slot] = dropped.value_or(file.data());
          applyCurrent();
        }
      }
    }
  }

} // namespace Tako

#endif // _DEBUG
