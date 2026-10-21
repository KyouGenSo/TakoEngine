#include "ShaderEditor.h"

#ifdef _DEBUG

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>

#include "AssetBrowser.h"
#include "DebugUIManager.h"
#include "DX12Basic.h"
#include "EnginePaths.h"
#include "ImGuiManager.h"
#include "MaterialLibrary.h"
#include "MaterialPropertyLayout.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "ShaderLibrary.h"
#include "StringUtility.h"

namespace Tako {

  using json = nlohmann::json;

  namespace {

    const std::filesystem::path kShaderDirectory = "resources/Shaders";
    constexpr const char*       kKindNames[]     = { "Object", "PostEffect" };

    // 新規作成のひな形。$NAME はシェーダー名に置き換える
    constexpr std::string_view kObjectTemplate = R"(// $NAME: テクスチャの色に gMaterial.color を掛ける
#include "Object3d.hlsli"
#include "Common.hlsli"
#include "View.hlsli"

struct Material
{
    float4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<View> gView : register(b2);
Texture2D<float4> gTexture : register(t0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    return gTexture.Sample(gLinearWrap, input.texcoord) * gMaterial.color;
}
)";

    constexpr std::string_view kPostEffectTemplate = R"(// $NAME: 画面の色に gParam.color を掛ける（gView.time・gView.screenSize も使える）
#include "FullScreen.hlsli"
#include "Common.hlsli"
#include "View.hlsli"

struct Param
{
    float4 color;
};

ConstantBuffer<Param> gParam : register(b0);
ConstantBuffer<View> gView : register(b1);
Texture2D<float4> gTexture : register(t0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    return gTexture.Sample(gLinearClamp, input.texCoord) * gParam.color;
}
)";

    std::optional<std::string> ReadText(const std::filesystem::path& path)
    {
      std::ifstream ifs(path);
      if (!ifs) {
        return std::nullopt;
      }
      return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }

    bool WriteText(const std::filesystem::path& path, std::string_view text)
    {
      std::ofstream ofs(path);
      if (!ofs) {
        LogToConsole(std::format("Shader Editor: Failed to write {}", path.generic_string()), DebugUIManager::LogType::Error);
        return false;
      }
      ofs << text;
      return true;
    }

    std::filesystem::file_time_type LastWriteTime(const std::filesystem::path& path)
    {
      std::error_code error;
      const auto time = std::filesystem::last_write_time(path, error);
      return error ? std::filesystem::file_time_type{} : time;
    }

    /// <summary>
    /// 文字列の項目。打ち終えたときだけ書き換える（途中の名前でコンパイルしない）
    /// </summary>
    bool TextField(const char* label, json& node, const char* key, const char* hint)
    {
      std::array<char, 256> buffer{};
      node.value(key, std::string()).copy(buffer.data(), buffer.size() - 1);
      ImGui::InputTextWithHint(label, hint, buffer.data(), buffer.size());
      if (!ImGui::IsItemDeactivatedAfterEdit()) {
        return false;
      }
      node[key] = buffer.data();
      return true;
    }

    /// <summary>
    /// 名前の表から選ぶ項目。書いていなければ fallback を選んでいるものとして出す
    /// </summary>
    template<size_t N>
    bool NameCombo(const char* label, json& node, const char* key, const char* const (&names)[N], const char* fallback)
    {
      const auto indexOf = [&](std::string_view text) {
        for (size_t i = 0; i < N; ++i) {
          if (text == names[i]) {
            return static_cast<int>(i);
          }
        }
        return -1;
      };
      int index = indexOf(node.value(key, std::string(fallback)));
      if (index < 0) {
        index = indexOf(fallback);
      }
      if (!ImGui::Combo(label, &index, names, static_cast<int>(N))) {
        return false;
      }
      node[key] = names[index];
      return true;
    }

    /// <summary>
    /// 灰色の折り返す説明文
    /// </summary>
    void DrawNote(const char* text)
    {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      ImGui::TextWrapped("%s", text);
      ImGui::PopStyleColor();
    }

    bool BoolField(const char* label, json& node, const char* key, bool fallback)
    {
      bool value = node.value(key, fallback);
      if (!ImGui::Checkbox(label, &value)) {
        return false;
      }
      node[key] = value;
      return true;
    }

  } // anonymous namespace

  std::span<const ShortcutBinding<ShaderEditor>> ShaderEditor::Shortcuts()
  {
    static constexpr ShortcutBinding<ShaderEditor> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_None, "Save the shader file", [](ShaderEditor& editor) { editor.Save(); } },
    };
    return kShortcuts;
  }

  void ShaderEditor::Draw()
  {
    if (!*isOpen_) {
      return;
    }

    ImGui::SetNextWindowSize(ImVec2(820.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Shader Editor", isOpen_)) {
      // 外のエディタで保存されたら読み直す（編集中なら上書きしない）
      if (!selected_.empty() && !tracker_.IsEdited() && LastWriteTime(selected_) != fileTime_) {
        Select(selected_);
      }

      // UI を描く区間の前後で保存内容を比べて編集を検出する
      tracker_.BeginSection();

      ImGui::SetNextWindowSizeConstraints(ImVec2(160.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
      if (ImGui::BeginChild("List##ShaderEditor", ImVec2(220.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
        DrawListPane();
      }
      ImGui::EndChild();

      ImGui::SameLine();
      if (ImGui::BeginChild("Shader##ShaderEditor", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
        if (!selected_.empty()) {
          DrawShaderPane();
        }
        else {
          ImGui::TextDisabled("Select or create a shader");
        }
      }
      ImGui::EndChild();

      unsavedPrompt_.Draw("Unsaved Changes##ShaderEditor", [this] { return Save(); });

      tracker_.EndSection();
      ProcessShortcuts(Shortcuts(), *this);
    }
    ImGui::End();
  }

  void ShaderEditor::CollectUnsavedChanges(std::vector<std::string>& out) const
  {
    if (tracker_.IsEdited()) {
      out.push_back(std::format("Shader Editor: {}", selected_));
    }
  }

  void ShaderEditor::Select(const std::string& path)
  {
    const std::optional<std::string> text = ReadText(path);
    json document = text ? json::parse(*text, nullptr, false) : json();
    if (!document.is_object()) {
      LogToConsole(std::format("Shader Editor: Failed to read {}", path), DebugUIManager::LogType::Error);
      return;
    }
    selected_ = path;
    document_ = std::move(document);
    fileTime_ = LastWriteTime(path);
    tracker_.Reset();
  }

  bool ShaderEditor::Save()
  {
    if (selected_.empty() || IsReadOnly() || !WriteText(selected_, document_.dump(4) + "\n")) {
      return false;
    }
    fileTime_ = LastWriteTime(selected_);
    tracker_.MarkSaved();
    LogToConsole(std::format("Shader saved: {}", selected_));
    return true;
  }

  void ShaderEditor::Create(const std::string& name, Kind kind)
  {
    std::error_code ec;
    std::filesystem::create_directories(kShaderDirectory, ec);
    const std::string psPath   = (kShaderDirectory / (name + ".PS.hlsl")).generic_string();
    const std::string jsonPath = (kShaderDirectory / (name + ".shader.json")).generic_string();

    std::string hlsl(kind == Kind::Object ? kObjectTemplate : kPostEffectTemplate);
    hlsl.replace(hlsl.find("$NAME"), 5, name);

    const json defaults = { { "color", { 1.0f, 1.0f, 1.0f, 1.0f } } };
    const json document = kind == Kind::Object
      ? json{ { "Name", name }, { "Fallback", "Lit" }, { "Forward", { { "VS", "Object3D.VS.hlsl" }, { "PS", psPath } } }, { "Defaults", defaults } }
      : json{ { "Name", name }, { "PostEffect", { { "PS", psPath } } }, { "Defaults", defaults } };
    if (!WriteText(psPath, hlsl) || !WriteText(jsonPath, document.dump(4) + "\n")) {
      return;
    }
    MaterialLibrary::GetInstance()->LoadShaderFile(jsonPath);
    Select(jsonPath);
  }

  void ShaderEditor::ApplyDocument()
  {
    if (!selected_.empty() && !IsReadOnly() && document_.is_object()) {
      MaterialLibrary::GetInstance()->ApplyShaderText(selected_, document_.dump());
    }
  }

  void ShaderEditor::DrawListPane()
  {
    ImGui::SeparatorText("Shaders");
    for (const std::string& path : MaterialLibrary::GetInstance()->GetShaderFiles()) {
      std::string label = std::filesystem::path(path).filename().string();
      label.resize(label.size() - std::string_view(".shader.json").size());
      if (path.starts_with(EnginePaths::kEngineRoot)) {
        label += "  (engine)";
      }
      if (ImGui::Selectable(std::format("{}###{}", label, path).c_str(), path == selected_) && path != selected_) {
        unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("Shader '{}' has unsaved changes.", selected_), "Open",
                                    [this, path] { Select(path); });
      }
    }

    // 作成と同時に保存して一覧に出す
    ImGui::SeparatorText("New");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##NewName", "name", newName_.data(), newName_.size());
    int kind = static_cast<int>(newKind_);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##NewKind", &kind, kKindNames, IM_ARRAYSIZE(kKindNames))) {
      newKind_ = static_cast<Kind>(kind);
    }

    // 名前はファイル名と登録名を兼ねるので、既にあるシェーダー・ポストエフェクトと重ねない
    const std::string name  = newName_.data();
    const bool        valid = std::ranges::all_of(name, [](unsigned char c) { return std::isalnum(c) || c == '_'; });
    const bool        taken = Renderer::GetInstance()->FindShader(name) || FindPostEffect(name) || std::filesystem::exists(kShaderDirectory / (name + ".shader.json"));
    ImGui::BeginDisabled(name.empty() || !valid || taken);
    if (ImGui::Button("Create", ImVec2(-1.0f, 0.0f))) {
      unsavedPrompt_.RunOrConfirm(tracker_.IsEdited(), std::format("Shader '{}' has unsaved changes.", selected_), "Create", [this, name] {
        Create(name, newKind_);
        newName_.fill('\0');
      });
    }
    ImGui::EndDisabled();
    if (!name.empty() && (!valid || taken)) {
      ImGui::TextDisabled(!valid ? "Use letters, digits and _" : "The name is already used");
    }
  }

  void ShaderEditor::DrawShaderPane()
  {
    const std::string name     = document_.value("Name", std::string());
    const bool        isPost   = document_.contains("PostEffect") && !document_.contains("Forward");
    const bool        readOnly = IsReadOnly();

    ImGui::SeparatorText(tracker_.IsEdited() ? std::format("{} *", name).c_str() : name.c_str());
    ImGui::BeginDisabled(readOnly);
    if (ImGui::Button("Save")) {
      Save();
    }
    ImGui::SameLine();
    tracker_.DrawButtons();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      ApplyDocument();
    }
    ImGui::SetItemTooltip("Compile the HLSL again and re-register (use after fixing a compile error)");

    ShaderLibrary&    library = Renderer::GetInstance()->GetDX12Basic()->GetShaderLibrary();
    const std::string ps      = FindPixelShader();
    if (!ps.empty()) {
      ImGui::SameLine();
      if (ImGui::Button("Open HLSL")) {
        OpenWithShell(library.ResolvePath(StringUtility::ConvertString(ps)));
      }
    }

    // 登録の状態と、最後のコンパイルエラー
    const bool registered = isPost ? FindPostEffect(name) != nullptr : Renderer::GetInstance()->FindShader(name) != nullptr;
    ImGui::TextDisabled(registered ? "Registered" : "Not registered");
    if (const std::string* error = ps.empty() ? nullptr : library.FindCompileError(StringUtility::ConvertString(ps))) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      ImGui::TextWrapped("%s", error->c_str());
      ImGui::PopStyleColor();
    }
    if (readOnly) {
      DrawNote("Engine file (build-time copy). Edit the source in engine/resources/shaders");
    }

    // PS をリフレクションした値の並び（描画物は gMaterial、ポストエフェクトは gParam）
    std::shared_ptr<const MaterialPropertyLayout> layout;
    if (isPost) {
      const PostEffectDefinition* definition = FindPostEffect(name);
      const PropertyBlock*        block      = definition ? std::get_if<PropertyBlock>(&definition->defaultParam) : nullptr;
      layout = block ? block->layout : nullptr;
    }
    else if (const ShaderAsset* shader = Renderer::GetInstance()->FindShader(name)) {
      layout = MaterialLibrary::GetInstance()->GetLayout(*shader);
    }

    ImGui::BeginDisabled(readOnly);
    bool changed = false;
    if (document_.contains("Forward")) {
      changed |= DrawForward();
    }
    if (document_.contains("PostEffect")) {
      changed |= DrawPostEffect(layout.get());
    }
    if (layout) {
      changed |= DrawProperties(*layout);
    }
    ImGui::EndDisabled();
    if (changed) {
      ApplyDocument();
    }
  }

  bool ShaderEditor::DrawForward()
  {
    json& forward = document_["Forward"];
    bool  changed = false;

    ImGui::SeparatorText("Object");
    changed |= TextField("VS", forward, "VS", "Object3D.VS.hlsl");
    changed |= TextField("PS", forward, "PS", "resources/Shaders/Name.PS.hlsl");
    ImGui::SetItemTooltip("A file name only is an engine shader. A path is relative to the working directory");

    // 書いていない項目は読み込み側（MaterialLibrary）の既定と同じものを選んでいるとして出す
    changed |= NameCombo("Blend", forward, "Blend", kBlendModeNames, "Alpha");
    changed |= NameCombo("Cull", forward, "Cull", kCullModeNames, "Back");
    changed |= BoolField("Depth Test", forward, "DepthTest", true);
    ImGui::SameLine();
    changed |= BoolField("Depth Write", forward, "DepthWrite", true);
    changed |= NameCombo("Depth Func", forward, "DepthFunc", kDepthFuncNames, "LessEqual");

    int queue = document_.value("Queue", ShaderAsset::kQueueGeometry);
    if (ImGui::DragInt("Queue", &queue, 1.0f, 0, 5000)) {
      document_["Queue"] = queue;
      changed = true;
    }
    ImGui::SetItemTooltip("Background 1000 / Geometry 2000 / Transparent 3000 (sorted back to front) / Overlay 4000");

    // 影・マスクのパスを借りる先
    const std::string name     = document_.value("Name", std::string());
    const std::string fallback = document_.value("Fallback", std::string());
    if (ImGui::BeginCombo("Fallback", fallback.empty() ? "(none)" : fallback.c_str())) {
      if (ImGui::Selectable("(none)", fallback.empty())) {
        document_.erase("Fallback");
        changed = true;
      }
      for (const std::string& shader : Renderer::GetInstance()->GetShaderNames()) {
        if (shader != name && ImGui::Selectable(shader.c_str(), shader == fallback)) {
          document_["Fallback"] = shader;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Shadow and mask passes are borrowed from this shader");
    DrawNote("Render state is copied when a material is created. Change existing materials in Material Editor");
    return changed;
  }

  bool ShaderEditor::DrawPostEffect(const MaterialPropertyLayout* layout)
  {
    json& post    = document_["PostEffect"];
    bool  changed = false;

    ImGui::SeparatorText("Post Effect");
    changed |= TextField("PS##Post", post, "PS", "resources/Shaders/Name.PS.hlsl");
    DrawNote("gTexture = screen, gView = time / screenSize / inverse matrices, gDepthTexture = scene depth");

    // gTexture（入力の画面）以外のテクスチャのファイル
    if (layout) {
      ImGui::PushID("Textures");
      json textures = post.value("Textures", json::object());
      for (const std::string& slot : layout->textures) {
        if (slot != "gTexture" && TextField(slot.c_str(), textures, slot.c_str(), "white.png")) {
          post["Textures"] = textures;
          changed = true;
        }
      }
      ImGui::PopID();
    }
    return changed;
  }

  bool ShaderEditor::DrawProperties(const MaterialPropertyLayout& layout)
  {
    bool changed = false;

    // 範囲と初期値は同じプロパティ名をラベルにするので、区間ごとに ID を分ける
    ImGui::SeparatorText("Properties (slider range)");
    ImGui::PushID("Ranges");
    json ranges = document_.value("Properties", json::object());
    for (const MaterialProperty& property : layout.properties) {
      if (property.type == MaterialProperty::Type::Unsupported || property.isColor) {
        continue;
      }
      float range[2] = { 0.0f, 0.0f };
      if (const auto it = ranges.find(property.name); it != ranges.end() && it->is_array() && it->size() == 2) {
        range[0] = (*it)[0].get<float>();
        range[1] = (*it)[1].get<float>();
      }
      if (ImGui::DragFloat2(property.name.c_str(), range, 0.01f)) {
        // min == max は範囲なし
        if (range[0] == range[1]) {
          ranges.erase(property.name);
        }
        else {
          ranges[property.name] = { range[0], range[1] };
        }
        document_["Properties"] = ranges;
        changed = true;
      }
    }
    ImGui::PopID();

    ImGui::SeparatorText("Defaults");
    ImGui::PushID("Defaults");
    std::vector<uint8_t> bytes(layout.constantSize);
    layout.DeserializeValues(document_.value("Defaults", json::object()), bytes);
    if (layout.DrawImGui(bytes)) {
      document_["Defaults"] = layout.SerializeValues(bytes);
      changed = true;
    }
    ImGui::PopID();
    return changed;
  }

  std::string ShaderEditor::FindPixelShader() const
  {
    for (const char* key : { "Forward", "PostEffect" }) {
      if (const auto it = document_.find(key); it != document_.end() && it->is_object()) {
        return it->value("PS", std::string());
      }
    }
    return {};
  }

  bool ShaderEditor::IsReadOnly() const
  {
    return selected_.starts_with(EnginePaths::kEngineRoot);
  }

} // namespace Tako

#endif // _DEBUG
