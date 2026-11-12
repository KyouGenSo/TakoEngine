#include "MaterialEditor.h"

#ifdef _DEBUG
#include "EditorUI.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <json.hpp>

#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "ImGuiManager.h"
#include "Material.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "OverrideField.h"
#include "PostProcess.h"
#include "PrimitiveBuilder.h"
#include "Renderer.h"
#include "ShaderLibrary.h"

namespace Tako {

  using json = nlohmann::json;

  MaterialEditor::MaterialEditor() = default;
  MaterialEditor::~MaterialEditor() = default;

  namespace {

    constexpr float kPreviewHeight = 260.0f;  ///< プレビュー画像の高さの初期値（px）

    /// <summary>
    /// プレビューに選べるプリミティブ
    /// </summary>
    struct PreviewPrimitive {
      const char*            name;
      std::unique_ptr<Model> (*create)();
    };

    const PreviewPrimitive kPreviewPrimitives[] = {
      { "Sphere",   [] { return PrimitiveBuilder::CreateSphere(); } },
      { "Cube",     [] { return PrimitiveBuilder::CreateCube(); } },
      { "Plane",    [] { return PrimitiveBuilder::CreatePlane(); } },
      { "Cylinder", [] { return PrimitiveBuilder::CreateCylinder(); } },
      { "Torus",    [] { return PrimitiveBuilder::CreateTorus(); } },
    };

    /// <summary>
    /// メッシュに貼れるシェーダーか（Sprite や SkyBox のシェーダーは、頂点の並びと受け取る値が違う）
    /// </summary>
    bool IsMeshShader(const ShaderAsset& shader)
    {
      const GraphicsPipelineDesc* forward = shader.GetPass(ShaderPass::Forward);
      return forward && forward->vertexLayout == VertexLayout::Mesh;
    }

    /// <summary>
    /// Forward のどれかのパスが gOpaqueTexture を読むか
    /// </summary>
    bool ReadsOpaqueTexture(const ShaderAsset& shader)
    {
      const auto                  declares = [](const GraphicsPipelineDesc& pass) { return pass.ps && pass.ps->Declares("gOpaqueTexture"); };
      const GraphicsPipelineDesc* forward  = shader.GetPass(ShaderPass::Forward);
      return (forward && declares(*forward)) || std::ranges::any_of(shader.GetExtraForwardPasses(), declares);
    }

    /// <summary>
    /// 名前の表から列挙の値を選ぶ。offset は「値 - 添字」（D3D12_COMPARISON_FUNC は 1 から始まる）
    /// </summary>
    template<class T, size_t N>
    bool EnumCombo(const char* label, T& value, const char* const (&names)[N], int offset = 0)
    {
      int index = static_cast<int>(value) - offset;
      if (!UI::Combo(label, &index, names, static_cast<int>(N))) {
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

  void MaterialEditor::Update()
  {
    if (!*isOpen_) {
      // エディタを閉じたらプレビューを破棄
      previewObject_.reset();
      floorObject_.reset();
      return;
    }

    // ポストエフェクトのマテリアルを選んでいる間だけ、プレビュー専用のスタックに 1 つ置く（値は preview_.Update が毎フレーム作り直す）
    MaterialLibrary*                                library = MaterialLibrary::GetInstance();
    const std::shared_ptr<const PostEffectMaterial> post    = library->IsPostEffectMaterial(selected_) ? library->LoadPostEffect(selected_) : nullptr;
    PostProcessStack&                               stack   = preview_.GetPostProcess();
    if ((stack.GetEffects().empty() ? nullptr : stack.GetEffects().front().material) != post) {
      stack.Clear();
      if (PostEffectInstance* effect = post ? stack.Find(stack.Add(post->shader)) : nullptr) {
        effect->material = post;
      }
    }

    preview_.Update();

    if (!floorObject_) {
      floorObject_ = std::make_unique<Object3d>();
      floorObject_->Initialize();
      floorObject_->SetModel(PrimitiveBuilder::CreatePlane({ .width = 10.0f, .height = 10.0f }));
      floorObject_->SetMaterialColor(Vector4(0.35f, 0.35f, 0.35f, 1.0f));
      floorObject_->SetTranslate(Vector3(0.0f, -1.0f, 0.0f));
    }

    if (!previewObject_) {
      previewObject_ = std::make_unique<Object3d>();
      previewObject_->Initialize();
      isMeshDirty_ = true;
    }

    // モデルの差し替えはコマンド未記録・GPU アイドルのここでのみ行う（ImGui ハンドラ内での差し替えは記録済みコマンドの参照先を壊す）
    if (isMeshDirty_) {
      if (previewModel_.empty()) {
        previewObject_->SetModel(kPreviewPrimitives[previewPrimitive_].create());
      }
      else {
        previewObject_->SetModel(previewModel_);
      }
      isMeshDirty_ = false;
    }

    // メッシュ用でないシェーダーとポストエフェクトのマテリアルは、モデルの既定のマテリアルで描く
    std::shared_ptr<Material> material = library->Find(selected_);
    if (material && !IsMeshShader(*material->GetShader())) {
      material.reset();
    }
    if (const Model* model = previewObject_->GetModel()) {
      for (size_t i = 0; i < model->GetMeshCount(); ++i) {
        previewObject_->SetMaterial(i, material);
      }
    }

    floorObject_->Update();
    previewObject_->Update();
  }

  void MaterialEditor::Finalize()
  {
    previewObject_.reset();
    floorObject_.reset();
    preview_.Finalize();
  }

  void MaterialEditor::DrawPreviewPass()
  {
    if (!*isOpen_ || !preview_.IsReady() || !previewObject_) {
      return;
    }

    preview_.BeginPass();
    preview_.Render([this] {
      floorObject_->Draw();
      previewObject_->Draw();
    });
    preview_.EndPass();
  }

  bool MaterialEditor::Draw()
  {
    if (!*isOpen_) {
      return false;
    }

    bool inputCaptured = false;
    ImGui::SetNextWindowSize(ImVec2(820.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(WindowTitle::kMaterialEditor, isOpen_)) {
      // UI を描く区間の前後で保存内容を比べて編集を検出する
      tracker_.BeginSection();

      ImGui::SetNextWindowSizeConstraints(ImVec2(160.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
      if (ImGui::BeginChild("List##MatEditor", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
        DrawListPane();
      }
      ImGui::EndChild();

      ImGui::SameLine();
      if (ImGui::BeginChild("Material##MatEditor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
        const std::shared_ptr<Material> material = MaterialLibrary::GetInstance()->Find(selected_);
        if (material || MaterialLibrary::GetInstance()->IsPostEffectMaterial(selected_)) {
          if (UI::IconButton(ICON_FA_FLOPPY_DISK, "Save (Ctrl+S)")) {
            Save();
          }
          ImGui::SameLine();
          ImGui::AlignTextToFramePadding();
          ImGui::Text("%s%s", selected_.c_str(), tracker_.IsEdited() ? " *" : "");

          inputCaptured = DrawPreview();

          // 設定だけをスクロールさせ、プレビューは見えたままにする
          if (ImGui::BeginChild("Settings##MatEditor")) {
            if (material) {
              DrawMaterialPane(*material);
            }
            else {
              DrawPostEffectPane();
            }
          }
          ImGui::EndChild();
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
    return inputCaptured;
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
    if (ImGui::Button(ICON_FA_PLUS "  Create", ImVec2(-1.0f, 0.0f))) {
      const std::string name = newName_.data();
      unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("Material '{}' has unsaved changes.", selected_), "Create", [this, name] {
        if (MaterialLibrary::GetInstance()->Create(name, newShader_) && MaterialLibrary::GetInstance()->Save(name)) {
          Select(name);
        }
      });
    }
    ImGui::EndDisabled();
  }

  bool MaterialEditor::DrawPreview()
  {
    bool inputCaptured = false;
    // 下端ドラッグで高さを調整できる。サイズ指定は初期値で、調整結果は imgui.ini に保存される
    if (ImGui::BeginChild("Preview##MatEditor", ImVec2(0.0f, kPreviewHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY)) {
      inputCaptured = preview_.DrawImage();
    }
    ImGui::EndChild();

    preview_.DrawCameraToolbar();
    ImGui::SameLine();
    ImGui::TextUnformatted("Mesh");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##PreviewMesh", previewModel_.empty() ? kPreviewPrimitives[previewPrimitive_].name : previewModel_.c_str(), ImGuiComboFlags_HeightLarge)) {
      for (size_t i = 0; i < std::size(kPreviewPrimitives); ++i) {
        if (ImGui::Selectable(kPreviewPrimitives[i].name, previewModel_.empty() && i == previewPrimitive_)) {
          previewPrimitive_ = i;
          previewModel_.clear();
          isMeshDirty_ = true;
        }
      }
      ImGui::SeparatorText("Models");
      for (const std::string& model : ModelManager::GetInstance()->GetLoadedModelNames()) {
        if (ImGui::Selectable(model.c_str(), model == previewModel_)) {
          previewModel_ = model;
          isMeshDirty_  = true;
        }
      }
      ImGui::EndCombo();
    }

    if (const std::shared_ptr<Material> material = MaterialLibrary::GetInstance()->Find(selected_)) {
      if (!IsMeshShader(*material->GetShader())) {
        ImGui::TextDisabled("This shader is not for meshes (showing the default material)");
      }
      else if (material->GetRenderQueue() < ShaderAsset::kQueueTransparent && ReadsOpaqueTexture(*material->GetShader())) {
        // 不透明の段の描画物には渡されず、ゲームでも黒を読む
        ImGui::TextDisabled("gOpaqueTexture is passed only to queue 3000+ (Transparent)");
      }
    }
    return inputCaptured;
  }

  void MaterialEditor::DrawMaterialPane(Material& material)
  {
    MaterialLibrary* library = MaterialLibrary::GetInstance();

    ImGui::TextDisabled("Assign to objects in Inspector > MeshComponent > Rendering");

    // シェーダーと描画設定・テクスチャのファイル名は保存内容を書き換えて当てはめる（値は名前で引き継がれる）
    json       current      = json::parse(library->Serialize(selected_));
    const auto applyCurrent = [&] { library->Apply(selected_, current.dump()); };

    if (UI::BeginCombo("Shader", material.GetShader()->GetName().c_str())) {
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
      return UI::DragInt(label, &value, 1.0f, 0, 5000);
    });
    ImGui::SetItemTooltip("Background 1000 / Geometry 2000 / Transparent 3000 (sorted back to front) / Overlay 4000");
    changed |= OverrideField<BlendMode>("Blend", state.blend, inherited.blend, [](const char* label, BlendMode& value) {
      return EnumCombo(label, value, kBlendModeNames);
    });
    changed |= OverrideField<CullMode>("Cull", state.cull, inherited.cull, [](const char* label, CullMode& value) {
      return EnumCombo(label, value, kCullModeNames);
    });
    changed |= OverrideField<bool>("Depth Test", state.depthTest, inherited.depthTest, [](const char* label, bool& value) {
      return UI::Checkbox(label, &value);
    });
    changed |= OverrideField<bool>("Depth Write", state.depthWrite, inherited.depthWrite, [](const char* label, bool& value) {
      return UI::Checkbox(label, &value);
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
        UI::InputTextWithHint(slot.c_str(), "white.png", file.data(), file.size());
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

    ImGui::TextDisabled("Add to the stack in PostEffect Editor (Materials). Edits apply to every effect that uses this material");

    // シェーダーとテクスチャのファイル名は保存内容を書き換えて当てはめる（値は名前で引き継がれる）
    json              current      = json::parse(library->Serialize(selected_));
    const std::string shader       = current.value("Shader", std::string());
    const auto        applyCurrent = [&] { library->Apply(selected_, current.dump()); };

    // 用途の違うシェーダーへは替えられない（描画物のマテリアルは新しく作る）
    if (UI::BeginCombo("Shader", shader.c_str())) {
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
        UI::InputTextWithHint(slot.c_str(), "(shader's texture)", file.data(), file.size());
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
