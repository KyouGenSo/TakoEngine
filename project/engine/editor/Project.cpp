#ifdef _DEBUG

#include "Project.h"
#include "StringUtility.h"

#include <Windows.h>
#include <ShlObj.h>
#include <shellapi.h>

#pragma comment(lib, "ole32.lib")

namespace Tako::Project {

  namespace {

    namespace fs = std::filesystem;

    constexpr const wchar_t* kDialogTitle = L"TakoEditor";

    fs::path root;

    std::string ToUtf8(const fs::path& path)
    {
      return StringUtility::ConvertString(path.wstring());
    }

    void ShowError(const std::string& message)
    {
      MessageBoxW(nullptr, StringUtility::ConvertString(message).c_str(), kDialogTitle, MB_OK | MB_ICONERROR);
    }

    /// <summary>
    /// エンジンのリソースを、作業フォルダーの EngineResources へ写す（ゲームのビルドの PreBuildEvent と同じ）
    /// </summary>
    bool CopyEngineResources(const fs::path& gameProject)
    {
      std::error_code error;
      fs::copy(GetEngineRoot() / "project" / "engine" / "resources", gameProject / "EngineResources", fs::copy_options::recursive | fs::copy_options::update_existing, error);
      return !error;
    }

    /// <summary>
    /// requested（ルートのフォルダーか .takoproject）のプロジェクトを開く
    /// </summary>
    /// <returns>開けなければ false（理由は表示してある）</returns>
    bool Open(const fs::path& requested)
    {
      std::error_code error;
      fs::path        folder = fs::weakly_canonical(requested, error);
      if (fs::is_regular_file(folder, error)) {
        folder = folder.parent_path();
      }
      root = folder;
      fs::current_path(root / "GameProject", error);
      if (error) {
        ShowError("GameProject was not found in:\n" + ToUtf8(root));
        return false;
      }
      // 普段はゲームのビルドが写す。ビルドする前でも起動できるよう、無ければここで写す
      if (!fs::exists("EngineResources", error) && !CopyEngineResources(root / "GameProject")) {
        ShowError("Could not copy the engine resources to:\n" + ToUtf8(root / "GameProject" / "EngineResources"));
        return false;
      }
      return true;
    }

  } // namespace

  std::optional<int> Startup()
  {
    int                       argc = 0;
    LPWSTR* const             argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> arguments;
    for (int i = 1; argv && i < argc; ++i) {
      arguments.emplace_back(argv[i]);
    }
    LocalFree(argv);

    fs::path requested;
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (arguments[i] == L"--project" && i + 1 < arguments.size()) {
        requested = arguments[i + 1];
      }
    }

    std::error_code error;
    if (requested.empty() && fs::is_directory("resources", error)) {
      // 作業フォルダーを GameProject にして起動されたとき
      requested = fs::current_path(error).parent_path();
    }
    if (requested.empty()) {
      ShowError("No project was specified. Start the editor with --project <project folder>");
      return 1;
    }
    if (!Open(requested)) {
      return 1;
    }
    return std::nullopt;
  }

  const fs::path& GetRoot()
  {
    return root;
  }

  fs::path GetEngineRoot()
  {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(GetModuleHandleW(L"TakoEngine.dll"), path, MAX_PATH);
    // <エンジン>/bin/Debug/TakoEngine.dll
    return fs::path(path).parent_path().parent_path().parent_path();
  }

  fs::path GetUserSettingsFolder()
  {
    PWSTR    appData = nullptr;
    fs::path folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
      folder = fs::path(appData) / L"TakoEngine";
    }
    // 失敗時も解放が必要
    CoTaskMemFree(appData);
    return folder;
  }

  fs::path GetGameModulePath()
  {
    // DLL でビルドするのは Debug だけ（TakoEngineCommon.props の TakoShared）
    return root / "Generated" / "outputs" / "Debug" / "Game.dll";
  }

  fs::path GetProjectFile()
  {
    std::vector<fs::path> found;
    std::error_code       error;
    for (const fs::directory_entry& entry : fs::directory_iterator(".", error)) {
      if (entry.is_regular_file() && entry.path().extension() == ".vcxproj") {
        found.push_back(entry.path());
      }
    }
    return found.size() == 1 ? found[0] : fs::path();
  }

} // namespace Tako::Project

#endif // _DEBUG
