#include "ShaderEditor.h"

#ifdef _DEBUG
#include "EditorUI.h"

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
      UI::InputTextWithHint(label, hint, buffer.data(), buffer.size());
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
      if (!UI::Combo(label, &index, names, static_cast<int>(N))) {
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
      if (!UI::Checkbox(label, &value)) {
        return false;
      }
      node[key] = value;
      return true;
    }

    constexpr const char* kFileInput = "(Textures file)";

    /// <summary>
    /// パスの見出しと、並べ替え・削除のボタン（押されたパスを raised・removed に入れる。反映は ApplyPassEdits）
    /// </summary>
    /// <param name="note">見出しの後ろに付ける説明</param>
    void DrawPassHeader(size_t index, size_t count, std::string_view note, std::optional<size_t>& raised, std::optional<size_t>& removed)
    {
      ImGui::TextUnformatted(std::format("Pass {}{}", index + 1, note).c_str());
      ImGui::SameLine();
      ImGui::BeginDisabled(index == 0);
      if (ImGui::SmallButton(ICON_FA_ARROW_UP "##Up")) {
        raised = index;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(index + 1 == count);
      if (ImGui::SmallButton(ICON_FA_ARROW_DOWN "##Down")) {
        raised = index + 1;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(count == 1);
      if (ImGui::SmallButton(ICON_FA_TRASH_CAN "##Remove")) {
        removed = index;
      }
      ImGui::SetItemTooltip("Remove this pass");
      ImGui::EndDisabled();
    }

    /// <summary>
    /// DrawPassHeader で押されたボタンを passes に反映する（raised は 1 つ前のパスと入れ替える）
    /// </summary>
    /// <returns>並びを変えたら true</returns>
    bool ApplyPassEdits(json& passes, std::optional<size_t> raised, std::optional<size_t> removed)
    {
      if (removed) {
        passes.erase(*removed);
        return true;
      }
      if (raised) {
        std::swap(passes[*raised - 1], passes[*raised]);
        return true;
      }
      return false;
    }

    /// <summary>
    /// パスの PS が宣言するテクスチャごとに、読む画像（Source・前のパスの Output・Textures のファイル）を選ぶ
    /// </summary>
    /// <returns>内容を変えたら true</returns>
    bool DrawPassInputs(ShaderLibrary& library, json& passes, size_t index)
    {
      json&             pass = passes[index];
      const std::string file = pass.value("PS", std::string());
      const Shader*     ps   = file.empty() ? nullptr : library.TryGet({ StringUtility::ConvertString(file), ShaderStage::Pixel, StringUtility::ConvertString(pass.value("Entry", std::string("main"))) });
      if (!ps) {
        return false;
      }

      std::vector<std::string> sources = { "Source" };
      for (size_t i = 0; i < index; ++i) {
        if (const std::string output = passes[i].value("Output", std::string()); !output.empty()) {
          sources.push_back(output);
        }
      }

      json inputs  = pass.value("Inputs", json::object());
      bool changed = false;
      for (const std::string& slot : MaterialPropertyLayout::Reflect(ps->GetReflection(), "gParam", {})->textures) {
        // 書かないときに読むもの（gTexture はエフェクトの入力、他は Textures のファイル）
        const bool        isMain   = slot == "gTexture";
        const std::string fallback = isMain ? "Source" : kFileInput;
        const std::string current  = inputs.value(slot, fallback);
        if (!UI::BeginCombo(slot.c_str(), current.c_str())) {
          continue;
        }
        std::vector<std::string> options = sources;
        if (!isMain) {
          options.insert(options.begin(), kFileInput);
        }
        for (const std::string& option : options) {
          if (ImGui::Selectable(option.c_str(), option == current) && option != current) {
            if (option == fallback) {
              inputs.erase(slot);
            }
            else {
              inputs[slot] = option;
            }
            changed = true;
          }
        }
        ImGui::EndCombo();
      }
      if (changed) {
        if (inputs.empty()) {
          pass.erase("Inputs");
        }
        else {
          pass["Inputs"] = inputs;
        }
      }
      return changed;
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
    if (ImGui::Begin(WindowTitle::kShaderEditor, isOpen_)) {
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
    if (ImGui::Button(ICON_FA_PLUS "  Create", ImVec2(-1.0f, 0.0f))) {
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

    ImGui::BeginDisabled(readOnly);
    if (UI::IconButton(ICON_FA_FLOPPY_DISK, readOnly ? "Engine shaders are read-only" : "Save (Ctrl+S)")) {
      Save();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (UI::IconButton(ICON_FA_ROTATE, "Compile the HLSL again and re-register (use after fixing a compile error)")) {
      ApplyDocument();
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s%s", name.c_str(), tracker_.IsEdited() ? " *" : "");

    ShaderLibrary&                 library = Renderer::GetInstance()->GetDX12Basic()->GetShaderLibrary();
    const std::vector<std::string> shaders = FindPixelShaders();
    for (size_t i = 0; i < shaders.size(); ++i) {
      ImGui::SameLine();
      const std::string label = shaders.size() == 1 ? std::string(ICON_FA_FILE_CODE "  Open HLSL") : std::format(ICON_FA_FILE_CODE "  Open {}##{}", std::filesystem::path(shaders[i]).filename().string(), i);
      if (ImGui::Button(label.c_str())) {
        OpenWithShell(library.ResolvePath(StringUtility::ConvertString(shaders[i])));
      }
    }

    // 登録の状態と、最後のコンパイル・読み込みのエラー
    const bool registered = isPost ? FindPostEffect(name) != nullptr : Renderer::GetInstance()->FindShader(name) != nullptr;
    ImGui::TextDisabled(registered ? "Registered" : "Not registered");
    ImGui::PushStyleColor(ImGuiCol_Text, GetEditorColors().error);
    for (const std::string& ps : shaders) {
      if (const std::string* error = library.FindCompileError(StringUtility::ConvertString(ps))) {
        ImGui::TextWrapped("%s", error->c_str());
      }
    }
    if (const std::string* error = MaterialLibrary::GetInstance()->FindLoadError(selected_)) {
      ImGui::TextWrapped("%s", error->c_str());
    }
    ImGui::PopStyleColor();
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
    DrawNote("Passes draw the same object in order. Materials override the render state of the first pass and own its values; later passes can read only those values and textures");

    // 1 つのオブジェクトは 1 パスとして見せ、編集したら配列で書く
    json                  passes       = forward.is_array() ? forward : json::array({ forward });
    bool                  passesEdited = false;
    std::optional<size_t> raised;
    std::optional<size_t> removed;
    for (size_t i = 0; i < passes.size(); ++i) {
      json& pass = passes[i];
      ImGui::PushID(static_cast<int>(i));
      DrawPassHeader(i, passes.size(), i == 0 ? " (material overrides apply)" : "", raised, removed);
      passesEdited |= TextField("VS", pass, "VS", "Object3D.VS.hlsl");
      passesEdited |= TextField("PS", pass, "PS", "resources/Shaders/Name.PS.hlsl");
      ImGui::SetItemTooltip("A file name only is an engine shader. A path is relative to the working directory");
      passesEdited |= TextField("Entry", pass, "Entry", "main");

      // 書いていない項目は読み込み側（MaterialLibrary）の既定と同じものを選んでいるとして出す
      passesEdited |= NameCombo("Blend", pass, "Blend", kBlendModeNames, "Alpha");
      passesEdited |= NameCombo("Cull", pass, "Cull", kCullModeNames, "Back");
      passesEdited |= BoolField("Depth Test", pass, "DepthTest", true);
      passesEdited |= BoolField("Depth Write", pass, "DepthWrite", true);
      passesEdited |= NameCombo("Depth Func", pass, "DepthFunc", kDepthFuncNames, "LessEqual");
      ImGui::PopID();
    }
    if (ImGui::Button(ICON_FA_PLUS "  Add Pass##Forward")) {
      json copy = passes.back();
      passes.push_back(std::move(copy));
      passesEdited = true;
    }
    passesEdited |= ApplyPassEdits(passes, raised, removed);
    if (passesEdited) {
      forward = std::move(passes);
      changed = true;
    }

    int queue = document_.value("Queue", ShaderAsset::kQueueGeometry);
    if (UI::DragInt("Queue", &queue, 1.0f, 0, 5000)) {
      document_["Queue"] = queue;
      changed = true;
    }
    ImGui::SetItemTooltip("Background 1000 / Geometry 2000 / Transparent 3000 (sorted back to front) / Overlay 4000");

    // 影・マスクのパスを借りる先
    const std::string name     = document_.value("Name", std::string());
    const std::string fallback = document_.value("Fallback", std::string());
    if (UI::BeginCombo("Fallback", fallback.empty() ? "(none)" : fallback.c_str())) {
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
    DrawNote("Materials use these values for the items they do not override in Material Editor");
    return changed;
  }

  bool ShaderEditor::DrawPostEffect(const MaterialPropertyLayout* layout)
  {
    json& post    = document_["PostEffect"];
    bool  changed = false;

    ImGui::SeparatorText("Post Effect");
    DrawNote("gTexture = Source (the effect input) unless Inputs says otherwise, gView = time / screenSize / inverse matrices, gDepthTexture = scene depth. Every pass shares gParam");

    // 書かなければ半透明の後のポストエフェクトの段
    constexpr const char* kStackEvent = "(post process stack)";
    const std::string     event       = post.value("Event", std::string());
    if (UI::BeginCombo("Event", event.empty() ? kStackEvent : event.c_str())) {
      if (ImGui::Selectable(kStackEvent, event.empty()) && !event.empty()) {
        post.erase("Event");
        changed = true;
      }
      for (const char* name : kRenderEventNames) {
        if (ImGui::Selectable(name, event == name) && event != name) {
          post["Event"] = name;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Where the effect is applied. Before the post process stack the input is the scene, after it the final image (AfterUI includes the UI). Effects with a mask ignore this");

    // "PS" だけの書き方は 1 パスとして見せ、編集したら Passes で書く
    json passes = post.value("Passes", json::array());
    if (passes.empty()) {
      json single = json::object();
      for (const char* key : { "PS", "Entry" }) {
        if (const auto it = post.find(key); it != post.end()) {
          single[key] = *it;
        }
      }
      passes.push_back(std::move(single));
    }

    ShaderLibrary&        library      = Renderer::GetInstance()->GetDX12Basic()->GetShaderLibrary();
    bool                  passesEdited = false;
    std::optional<size_t> raised;
    std::optional<size_t> removed;
    for (size_t i = 0; i < passes.size(); ++i) {
      json&      pass = passes[i];
      const bool last = i + 1 == passes.size();
      ImGui::PushID(static_cast<int>(i));
      DrawPassHeader(i, passes.size(), last ? " (writes the result)" : "", raised, removed);

      passesEdited |= TextField("PS", pass, "PS", "resources/Shaders/Name.PS.hlsl");
      ImGui::SetItemTooltip("A file name only is an engine shader. A path is relative to the working directory");
      passesEdited |= TextField("Entry", pass, "Entry", "main");
      if (!last) {
        passesEdited |= TextField("Output", pass, "Output", "name read by later passes");
        float scale = pass.value("Scale", 1.0f);
        if (UI::DragFloat("Scale", &scale, 0.01f, 0.01f, 4.0f)) {
          pass["Scale"] = scale;
          passesEdited = true;
        }
        ImGui::SetItemTooltip("Size relative to the effect input (0.5 = half resolution)");

        constexpr const char* kInputFormat = "(same as input)";
        const std::string     formatName   = pass.value("Format", std::string());
        if (UI::BeginCombo("Format", formatName.empty() ? kInputFormat : formatName.c_str())) {
          if (ImGui::Selectable(kInputFormat, formatName.empty()) && !formatName.empty()) {
            pass.erase("Format");
            passesEdited = true;
          }
          for (const auto& known : kPostEffectFormats) {
            if (ImGui::Selectable(known.first, formatName == known.first) && formatName != known.first) {
              pass["Format"] = known.first;
              passesEdited = true;
            }
          }
          ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Format of the render target this pass writes. Fewer channels and bits use less memory. FLOAT keeps values above 1 and faint gradients. A channel the format lacks reads as 0 (alpha as 1)");
      }
      passesEdited |= DrawPassInputs(library, passes, i);
      ImGui::PopID();
    }

    if (ImGui::Button(ICON_FA_PLUS "  Add Pass##Post")) {
      // 新しいパスが結果を書くので、今の最後のパスには名前を付けて新しいパスから読む
      json& previous = passes.back();
      if (previous.value("Output", std::string()).empty()) {
        previous["Output"] = std::format("pass{}", passes.size());
      }
      json added = { { "PS", previous.value("PS", std::string()) }, { "Inputs", { { "gTexture", previous["Output"] } } } };
      passes.push_back(std::move(added));
      passesEdited = true;
    }
    passesEdited |= ApplyPassEdits(passes, raised, removed);
    if (passesEdited) {
      post.erase("PS");
      post.erase("Entry");
      post["Passes"] = std::move(passes);
      changed = true;
    }

    // Inputs で選ばなかったテクスチャのファイル（全パス共通）
    if (layout && !layout->textures.empty()) {
      ImGui::SeparatorText("Textures");
      ImGui::PushID("Textures");
      json textures = post.value("Textures", json::object());
      for (const std::string& slot : layout->textures) {
        bool edited = TextField(slot.c_str(), textures, slot.c_str(), "white.png");
        if (const std::optional<std::string> dropped = AcceptAssetDrop("Texture")) {
          textures[slot] = *dropped;
          edited = true;
        }
        if (edited) {
          post["Textures"] = textures;
          changed = true;
        }
      }
      ImGui::PopID();
    }

    // 一時エフェクトで時間とともに弱める値（float の値だけ選べる）
    if (layout && std::ranges::any_of(layout->properties, [](const MaterialProperty& property) { return property.type == MaterialProperty::Type::Float; })) {
      ImGui::SeparatorText("Fade");
      DrawNote("Checked values go down to 0 over the duration of a temporary effect (PostEffectManager::ApplyTemporaryEffect)");
      json fade = post.value("Fade", json::array());
      if (fade.is_string()) {
        fade = json::array({ fade });
      }
      bool fadeEdited = false;
      for (const MaterialProperty& property : layout->properties) {
        if (property.type != MaterialProperty::Type::Float) {
          continue;
        }
        const auto it = std::find(fade.begin(), fade.end(), json(property.name));
        bool       on = it != fade.end();
        if (UI::Checkbox(std::format("{}##Fade", property.name).c_str(), &on)) {
          if (on) {
            fade.push_back(property.name);
          }
          else {
            fade.erase(it);
          }
          fadeEdited = true;
        }
      }
      if (fadeEdited) {
        if (fade.empty()) {
          post.erase("Fade");
        }
        else {
          post["Fade"] = fade;
        }
        changed = true;
      }
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
      if (UI::DragFloat2(property.name.c_str(), range, 0.01f)) {
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

  std::vector<std::string> ShaderEditor::FindPixelShaders() const
  {
    std::vector<std::string> files;
    const auto add = [&files](const json& node) {
      const std::string ps = node.is_object() ? node.value("PS", std::string()) : std::string();
      if (!ps.empty() && std::ranges::find(files, ps) == files.end()) {
        files.push_back(ps);
      }
    };
    // Forward はパスの配列でもよい
    if (const auto forward = document_.find("Forward"); forward != document_.end()) {
      if (forward->is_array()) {
        for (const json& pass : *forward) {
          add(pass);
        }
      }
      else {
        add(*forward);
      }
    }
    if (const auto post = document_.find("PostEffect"); post != document_.end() && post->is_object()) {
      add(*post);
      for (const json& pass : post->value("Passes", json::array())) {
        add(pass);
      }
    }
    return files;
  }

  bool ShaderEditor::IsReadOnly() const
  {
    return selected_.starts_with(EnginePaths::kEngineRoot);
  }

} // namespace Tako

#endif // _DEBUG
