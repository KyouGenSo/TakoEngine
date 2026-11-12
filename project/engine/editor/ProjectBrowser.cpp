#ifdef _DEBUG

#include "ProjectBrowser.h"
#include "DebugUIManager.h"
#include "EditorUI.h"
#include "Project.h"
#include "StringUtility.h"

#include "imgui.h"

#include <cstring>
#include <filesystem>
#include <format>
#include <string>

namespace Tako {

  namespace {

    namespace fs = std::filesystem;

    constexpr const char* kPopupName  = "New Project";
    constexpr float       kFieldWidth = 460.0f;

    bool        isPopupRequested = false;
    std::string popupError;
    char        nameBuffer[64]{};
    char        locationBuffer[512]{};

    std::string ToUtf8(const fs::path& path)
    {
      return StringUtility::ConvertString(path.wstring());
    }

    fs::path FromUtf8(const std::string& text)
    {
      return fs::path(StringUtility::ConvertString(text));
    }

    void OpenProject(const fs::path& root)
    {
      if (Project::OpenInNewEditor(root)) {
        LogToConsole(std::format("Opening {} in a new editor", ToUtf8(root)));
      }
      else {
        LogToConsole(std::format("Could not start a new editor for {}", ToUtf8(root)), DebugUIManager::LogType::Error);
      }
    }

  } // namespace

  void RequestNewProjectPopup()
  {
    isPopupRequested = true;
  }

  void DrawNewProjectPopup()
  {
    if (isPopupRequested) {
      isPopupRequested = false;
      popupError.clear();
      nameBuffer[0] = '\0';
      // 今のプロジェクトの隣を既定にする
      strncpy_s(locationBuffer, ToUtf8(Project::GetRoot().parent_path()).c_str(), _TRUNCATE);
      ImGui::OpenPopup(kPopupName);
    }
    if (!ImGui::BeginPopupModal(kPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      return;
    }

    ImGui::TextDisabled("Name");
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(kFieldWidth);
    const bool isEntered = ImGui::InputTextWithHint("##Name", "MyGame", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled("Location");
    ImGui::SetNextItemWidth(kFieldWidth);
    ImGui::InputText("##Location", locationBuffer, sizeof(locationBuffer));
    ImGui::PushTextWrapPos(kFieldWidth);
    ImGui::TextDisabled("%s", std::format("Creates {} and links it to this engine (TakoEngine {})", ToUtf8(FromUtf8(locationBuffer) / (nameBuffer[0] ? nameBuffer : "<name>")),
                                         Project::GetEngineVersion()).c_str());
    ImGui::PopTextWrapPos();
    if (!popupError.empty()) {
      ImGui::PushTextWrapPos(kFieldWidth);
      ImGui::TextColored(GetEditorColors().warning, "%s", popupError.c_str());
      ImGui::PopTextWrapPos();
    }

    const bool isCreatePressed = ImGui::Button(ICON_FA_PLUS "  Create");
    ImGui::SameLine();
    const bool isCanceled = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    bool       isCreated  = false;
    if (isEntered || isCreatePressed) {
      const fs::path location = FromUtf8(locationBuffer);
      popupError              = Project::Create(nameBuffer, location);
      if (popupError.empty()) {
        OpenProject(location / nameBuffer);
        isCreated = true;
      }
    }
    if (isCreated || isCanceled) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  void DrawProjectMenuItems()
  {
    if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS "  New Project...")) {
      RequestNewProjectPopup();
    }
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open Project...")) {
      if (const fs::path root = Project::BrowseForProject(); !root.empty()) {
        OpenProject(root);
      }
    }
    if (ImGui::BeginMenu(ICON_FA_CLOCK_ROTATE_LEFT "  Open Recent")) {
      for (const fs::path& root : Project::GetRecentProjects()) {
        // 今開いているプロジェクトは選べない（同じプロジェクトを 2 つのエディタで開かない）
        if (ImGui::MenuItem(ToUtf8(root).c_str(), nullptr, false, root != Project::GetRoot())) {
          OpenProject(root);
        }
      }
      ImGui::EndMenu();
    }
  }

} // namespace Tako

#endif // _DEBUG
