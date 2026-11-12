#ifdef _DEBUG

#include "NewScript.h"
#include "EditorUI.h"
#include "DebugUIManager.h"
#include "Project.h"
#include "TypeRegistry.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>

namespace Tako {

  namespace {

    namespace fs = std::filesystem;

    constexpr const char* kPopupName = "New Script";

    std::string pendingDirectory;
    std::string popupDirectory;
    std::string popupError;
    char        nameBuffer[64]{};

    constexpr std::string_view kHeaderTemplate =
      "#pragma once\n"
      "#include \"Script.h\"\n"
      "\n"
      "/// <summary>\n"
      "/// \n"
      "/// </summary>\n"
      "TCLASS()\n"
      "class {0} : public Tako::Script {{\n"
      "    GENERATED_BODY()\n"
      "\n"
      "public: //メンバー関数\n"
      "    void BeginPlay() override;\n"
      "\n"
      "    void Tick(float deltaTime) override;\n"
      "\n"
      "private: //メンバー変数\n"
      "    TPROPERTY(Edit, Speed = 0.05)\n"
      "    float speed_ = 1.0f;\n"
      "}};\n";

    constexpr std::string_view kSourceTemplate =
      "#include \"{0}.h\"\n"
      "#include \"Actor.h\"\n"
      "\n"
      "void {0}::BeginPlay() {{\n"
      "}}\n"
      "\n"
      "void {0}::Tick([[maybe_unused]] float deltaTime) {{\n"
      "}}\n";

    bool IsIdentifier(const std::string& name)
    {
      if (name.empty() || std::isdigit(static_cast<unsigned char>(name[0]))) { return false; }
      return std::ranges::all_of(name, [](unsigned char c) { return std::isalnum(c) || c == '_'; });
    }

    std::optional<std::string> ReadFile(const fs::path& path)
    {
      std::ifstream file(path, std::ios::binary);
      if (!file) { return std::nullopt; }
      return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    bool WriteFile(const fs::path& path, const std::string& text)
    {
      std::ofstream file(path, std::ios::binary);
      file << text;
      return file.good();
    }

    /// <summary>
    /// 最初の &lt;tag Include=" の行の前に、同じ字下げで lines（\n 区切り）を足す。改行は text の物に合わせる
    /// </summary>
    /// <returns>tag の行が無ければ false</returns>
    bool InsertBeforeFirst(std::string& text, const std::string& tag, const std::string& lines)
    {
      const size_t found = text.find("<" + tag + " Include=\"");
      if (found == std::string::npos) { return false; }
      const size_t      lineStart = text.rfind('\n', found) + 1;
      const std::string indent    = text.substr(lineStart, found - lineStart);
      const std::string newline   = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
      std::string       block;
      size_t            begin = 0;
      while (begin < lines.size()) {
        size_t end = lines.find('\n', begin);
        if (end == std::string::npos) { end = lines.size(); }
        block += indent + lines.substr(begin, end - begin) + newline;
        begin = end + 1;
      }
      text.insert(lineStart, block);
      return true;
    }

  } // namespace

  std::string CreateScriptFiles(const std::string& name, const std::string& directory)
  {
    if (!IsIdentifier(name)) {
      return "The name must be a C++ identifier (letters, digits and _; not starting with a digit)";
    }
    if (TypeRegistry::GetInstance()->Find(name)) {
      return std::format("A type named {} already exists", name);
    }
    const fs::path header = fs::path(directory) / (name + ".h");
    const fs::path source = fs::path(directory) / (name + ".cpp");
    std::error_code error;
    if (fs::exists(header, error) || fs::exists(source, error)) {
      return std::format("{} already exists", header.generic_string());
    }
    const fs::path project = Project::GetProjectFile();
    if (project.empty()) {
      return "The working folder must contain exactly one .vcxproj";
    }
    fs::path filters = project;
    filters += ".filters";
    std::optional<std::string> projectText = ReadFile(project);
    std::optional<std::string> filtersText = ReadFile(filters);
    if (!projectText || !filtersText) {
      return std::format("Could not read {} or its .filters", project.generic_string());
    }

    // プロジェクトファイルのパスは \ 区切り
    std::string headerItem = header.generic_string();
    std::string sourceItem = source.generic_string();
    std::ranges::replace(headerItem, '/', '\\');
    std::ranges::replace(sourceItem, '/', '\\');
    std::string filter = fs::path(directory).generic_string();
    std::ranges::replace(filter, '/', '\\');
    // フィルターが無ければ、フィルターなし（ソリューション エクスプローラーの直下）に置く
    const bool hasFilter = filtersText->find("<Filter Include=\"" + filter + "\">") != std::string::npos;
    const auto filterItem = [&](const char* tag, const std::string& item) {
      return hasFilter ? std::format("<{0} Include=\"{1}\">\n  <Filter>{2}</Filter>\n</{0}>", tag, item, filter)
                       : std::format("<{} Include=\"{}\" />", tag, item);
    };
    if (!InsertBeforeFirst(*projectText, "ClCompile", std::format("<ClCompile Include=\"{}\" />", sourceItem))
        || !InsertBeforeFirst(*projectText, "ClInclude", std::format("<ClInclude Include=\"{}\" />", headerItem))
        || !InsertBeforeFirst(*filtersText, "ClCompile", filterItem("ClCompile", sourceItem))
        || !InsertBeforeFirst(*filtersText, "ClInclude", filterItem("ClInclude", headerItem))) {
      return std::format("{} has no ClCompile / ClInclude item to insert next to", project.generic_string());
    }

    fs::create_directories(directory, error);
    if (!WriteFile(header, std::format(kHeaderTemplate, name)) || !WriteFile(source, std::format(kSourceTemplate, name))) {
      return std::format("Could not write {}", header.generic_string());
    }
    if (!WriteFile(project, *projectText) || !WriteFile(filters, *filtersText)) {
      return std::format("Could not write {}", project.generic_string());
    }
    return {};
  }

  void RequestNewScriptPopup(const std::string& directory)
  {
    pendingDirectory = directory;
  }

  void DrawNewScriptPopup()
  {
    if (!pendingDirectory.empty()) {
      popupDirectory = std::move(pendingDirectory);
      pendingDirectory.clear();
      popupError.clear();
      nameBuffer[0] = '\0';
      ImGui::OpenPopup(kPopupName);
    }
    if (!ImGui::BeginPopupModal(kPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      return;
    }
    bool isCreated = false;
    ImGui::TextDisabled("%s/", popupDirectory.c_str());
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool entered = ImGui::InputTextWithHint("##Name", "ClassName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    if (!popupError.empty()) {
      ImGui::TextColored(GetEditorColors().warning, "%s", popupError.c_str());
    }
    const bool create = ImGui::Button(ICON_FA_PLUS "  Create");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (entered || create) {
      popupError = CreateScriptFiles(nameBuffer, popupDirectory);
      if (popupError.empty()) {
        DebugUIManager::GetInstance()->AddLog(
          std::format("Created {0}/{1}.h and {1}.cpp (Visual Studio asks to reload the project)", popupDirectory, nameBuffer),
          DebugUIManager::LogType::Info);
        // 新しい型は、ビルドして DLL を差し替えると一覧に出る
        DebugUIManager::GetInstance()->GetScriptCompiler().Compile();
        isCreated = true;
      }
    }
    if (isCreated || cancel) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

} // namespace Tako

#endif // _DEBUG
