#ifdef _DEBUG

#include "Project.h"
#include "ChildProcess.h"
#include "Logger.h"
#include "ProjectMigration.h"
#include "StringUtility.h"

#include <json.hpp>

#include <Windows.h>
#include <ShlObj.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

namespace Tako::Project {

  namespace {

    namespace fs = std::filesystem;

    using nlohmann::json;

    constexpr const wchar_t* kDialogTitle       = L"TakoEditor";
    constexpr const char*    kMarkerExtension   = ".takoproject";
    constexpr size_t         kMaxRecentProjects = 10;

    // 新しいプロジェクトに作っておく空のフォルダー（git は空のフォルダーを持てないので、テンプレートには入れられない）
    constexpr const char* kEmptyFolders[] = {
      "GameProject/Source",
      "GameProject/resources/Texture",
      "GameProject/resources/Model",
      "GameProject/resources/Sound",
      "GameProject/resources/Fonts",
      "GameProject/resources/Shaders",
      "GameProject/resources/Materials",
      "GameProject/resources/Json/Prefabs",
      "GameProject/resources/Json/GameSettings",
      "GameProject/resources/Json/ParticlePresets",
      "GameProject/resources/Json/PostEffects",
      "GameProject/resources/Json/UI",
      "GameProject/resources/Json/BT",
      "GameProject/resources/Json/Animations",
      "GameProject/resources/Json/CameraAnimations",
    };

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
    /// folder にある .takoproject
    /// </summary>
    /// <returns>無ければ空</returns>
    fs::path FindMarker(const fs::path& folder)
    {
      std::error_code error;
      for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == kMarkerExtension) {
          return entry.path();
        }
      }
      return {};
    }

    /// <summary>
    /// 無いファイルと読めない中身は null を返す
    /// </summary>
    json ReadJson(const fs::path& path)
    {
      std::ifstream file(path);
      json          value = file ? json::parse(file, nullptr, false) : json();
      return value.is_discarded() ? json() : value;
    }

    bool WriteJson(const fs::path& path, const json& value)
    {
      std::error_code error;
      fs::create_directories(path.parent_path(), error);
      std::ofstream file(path);
      file << value.dump(2) << "\n";
      return file.good();
    }

    fs::path RecentFile()
    {
      return GetUserSettingsFolder() / "RecentProjects.json";
    }

    void AddRecent(const fs::path& project)
    {
      std::vector<fs::path> recent = GetRecentProjects();
      std::erase(recent, project);
      recent.insert(recent.begin(), project);
      recent.resize((std::min)(recent.size(), kMaxRecentProjects));
      json list = json::array();
      for (const fs::path& path : recent) {
        list.push_back(ToUtf8(path));
      }
      WriteJson(RecentFile(), list);
    }

    bool IsValidName(const std::string& name)
    {
      if (name.empty() || name.size() > 64 || std::isdigit(static_cast<unsigned char>(name[0]))) {
        return false;
      }
      return std::ranges::all_of(name, [](unsigned char c) { return std::isalnum(c) || c == '_'; });
    }

    void ReplaceAll(std::string& text, std::string_view from, std::string_view to)
    {
      for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
      }
    }

    /// <summary>
    /// XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX の形の新しい GUID
    /// </summary>
    std::string NewGuid()
    {
      GUID guid{};
      CoCreateGuid(&guid);
      wchar_t text[40]{};
      StringFromGUID2(guid, text, 40);
      // 前後の { } を外す
      return StringUtility::ConvertString(std::wstring(text + 1, 36));
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
    /// .takoproject を読み、保存形式が古ければ直し、エンジンのバージョンを今の物に進める
    /// </summary>
    /// <returns>開いてよければ true（だめな理由は表示してある）</returns>
    bool CheckVersions()
    {
      const fs::path marker = FindMarker(root);
      if (marker.empty()) {
        // .takoproject の無いプロジェクトは、今の形式として開く
        return true;
      }
      json data = ReadJson(marker);
      if (!data.is_object()) {
        data = json::object();
      }
      const int         formatVersion = data.value("formatVersion", kFormatVersion);
      const std::string projectEngine = data.value("engineVersion", std::string());
      const std::string engine        = GetEngineVersion();

      if (formatVersion > kFormatVersion) {
        ShowError(std::format("This project uses save format {}, but this engine (TakoEngine {}) reads up to format {}.\n"
                              "Update the engine with UpdateEngine.bat in the project folder.", formatVersion, engine, kFormatVersion));
        return false;
      }
      if (!projectEngine.empty() && !engine.empty() && CompareVersions(projectEngine, engine) > 0) {
        const std::wstring message = StringUtility::ConvertString(std::format(
          "This project was last opened with TakoEngine {}, and this engine is {} (older).\n"
          "Run UpdateEngine.bat in the project folder to update the engine.\n\nOpen it anyway?", projectEngine, engine));
        if (MessageBoxW(nullptr, message.c_str(), kDialogTitle, MB_YESNO | MB_ICONWARNING) != IDYES) {
          return false;
        }
      }
      if (formatVersion < kFormatVersion) {
        const std::wstring message = StringUtility::ConvertString(std::format(
          "The saved files of this project are in an older format ({}). They will be converted to format {}.\n"
          "A copy of resources/Json is saved to Generated/Backup first.", formatVersion, kFormatVersion));
        if (MessageBoxW(nullptr, message.c_str(), kDialogTitle, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) {
          return false;
        }
        const std::string error = ProjectMigration::Run(root, formatVersion);
        if (!error.empty()) {
          ShowError("The project could not be converted:\n" + error);
          return false;
        }
      }

      // 新しいエンジンで開いたら番号を進める（チームの他の人が古いエンジンで開くと、上の確認が出る）
      const bool isEngineNewer = !engine.empty() && (projectEngine.empty() || CompareVersions(engine, projectEngine) > 0);
      if (formatVersion < kFormatVersion || isEngineNewer) {
        data["formatVersion"] = kFormatVersion;
        if (isEngineNewer) {
          data["engineVersion"] = engine;
        }
        WriteJson(marker, data);
      }
      return true;
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
      if (!CheckVersions()) {
        return false;
      }
      // 普段はゲームのビルドが写す。ビルドする前でも起動できるよう、無ければここで写す
      if (!fs::exists("EngineResources", error) && !CopyEngineResources(root / "GameProject")) {
        ShowError("Could not copy the engine resources to:\n" + ToUtf8(root / "GameProject" / "EngineResources"));
        return false;
      }
      AddRecent(root);
      return true;
    }

    /// <summary>
    /// 場所と名前を選ぶダイアログを出し、作る場所を確かめてから、そこにプロジェクトを作る
    /// </summary>
    /// <returns>作ったプロジェクトのルート。取り消したら空</returns>
    fs::path BrowseForNewProject()
    {
      // 最初はドキュメントを開く（エディタの exe があるエンジンのフォルダーから始めない）
      PWSTR        documents = nullptr;
      std::wstring initialFolder;
      if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents))) {
        initialFolder = documents;
      }
      // 失敗時も解放が必要
      CoTaskMemFree(documents);

      // 作れるまで、場所を選び直せる
      while (true) {
        wchar_t       buffer[MAX_PATH] = L"MyGame";
        OPENFILENAMEW dialog{};
        dialog.lStructSize     = sizeof(dialog);
        dialog.lpstrTitle      = L"New Project: choose the folder and type the project name";
        dialog.lpstrFile       = buffer;
        dialog.nMaxFile        = MAX_PATH;
        dialog.lpstrInitialDir = initialFolder.c_str();
        dialog.Flags           = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&dialog)) {
          return {};
        }
        const fs::path     target  = buffer;
        const std::wstring message = StringUtility::ConvertString(std::format(
          "Create the project here?\n\n    {}\n\nIt uses this engine by a link (not a copy):\n    {}\n\nNo: choose another location",
          ToUtf8(target), ToUtf8(GetEngineRoot())));
        const int answer = MessageBoxW(nullptr, message.c_str(), kDialogTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDCANCEL) {
          return {};
        }
        if (answer == IDYES) {
          const std::string error = Create(ToUtf8(target.filename()), target.parent_path());
          if (error.empty()) {
            return target;
          }
          ShowError(error);
        }
      }
    }

    /// <summary>
    /// 開くプロジェクトが決まらないときに、開くか作るかを聞く
    /// </summary>
    /// <returns>プロジェクトのルート。取り消したら空</returns>
    fs::path AskForProject()
    {
      const int answer = MessageBoxW(nullptr,
                                     L"No project was specified.\n\nYes: open an existing project (.takoproject)\nNo: create a new project\nCancel: quit",
                                     kDialogTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
      if (answer == IDYES) {
        return BrowseForProject();
      }
      return answer == IDNO ? BrowseForNewProject() : fs::path();
    }

  } // namespace

  std::optional<int> Startup()
  {
    assert(CompareVersions("2.10.0", "2.9.9") > 0 && CompareVersions("v2.1", "2.1.0") == 0 && CompareVersions("2.1.0", "2.1.1") < 0);

    int                       argc = 0;
    LPWSTR* const             argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> arguments;
    for (int i = 1; argv && i < argc; ++i) {
      arguments.emplace_back(argv[i]);
    }
    LocalFree(argv);

    fs::path requested;
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (arguments[i] == L"--new" && i + 2 < arguments.size()) {
        const std::string error = Create(StringUtility::ConvertString(arguments[i + 2]), arguments[i + 1]);
        if (!error.empty()) {
          // スクリプトや CI から呼ばれるので、窓を出さずに標準エラーへ書く
          const std::string line    = "TakoEditor --new: " + error + "\n";
          DWORD             written = 0;
          Logger::Log(line);
          WriteFile(GetStdHandle(STD_ERROR_HANDLE), line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        }
        return error.empty() ? 0 : 1;
      }
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
      requested = AskForProject();
      if (requested.empty()) {
        return 0;
      }
    }
    if (!Open(requested)) {
      return 1;
    }
    return std::nullopt;
  }

  std::string Create(const std::string& name, const fs::path& location)
  {
    if (!IsValidName(name)) {
      return "The project name must be letters, digits and _ (not starting with a digit, up to 64 characters)";
    }
    std::error_code error;
    const fs::path  target       = fs::weakly_canonical(location, error) / name;
    const fs::path  engineRoot   = GetEngineRoot();
    const fs::path  templateRoot = engineRoot / "templates" / "Default";
    if (!fs::is_directory(templateRoot, error)) {
      return "The project template was not found: " + ToUtf8(templateRoot);
    }
    // エンジンのフォルダーの中に作ると、プロジェクトのエンジンへのリンクが自分の親を指す
    for (fs::path folder = target; folder.has_relative_path(); folder = folder.parent_path()) {
      if (fs::equivalent(folder, engineRoot, error)) {
        return "A project cannot be created inside the engine folder (" + ToUtf8(engineRoot) + "). Choose another location";
      }
    }
    if (fs::exists(target, error) && !fs::is_empty(target, error)) {
      return "The folder already exists and is not empty: " + ToUtf8(target);
    }

    const std::string guid    = NewGuid();
    const std::string version = GetEngineVersion();
    const auto        fill    = [&](std::string text) {
      ReplaceAll(text, "__ProjectName__", name);
      ReplaceAll(text, "__GUID__", guid);
      ReplaceAll(text, "__EngineVersion__", version);
      return text;
    };

    for (fs::recursive_directory_iterator it(templateRoot, error), end; !error && it != end; it.increment(error)) {
      const fs::path destination = target / fs::path(StringUtility::ConvertString(fill(ToUtf8(fs::relative(it->path(), templateRoot, error)))));
      if (it->is_directory(error)) {
        fs::create_directories(destination, error);
        continue;
      }
      fs::create_directories(destination.parent_path(), error);
      // ponytail: 全部のファイルをバイト列のまま置き換える（BOM と改行コードが残る）。テンプレートに大きいバイナリを入れるなら、拡張子で分ける
      std::ifstream input(it->path(), std::ios::binary);
      std::string   content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
      std::ofstream output(destination, std::ios::binary);
      output << fill(std::move(content));
      if (!output.good()) {
        return "Could not write " + ToUtf8(destination);
      }
    }
    if (error) {
      return "Could not read the project template: " + ToUtf8(templateRoot);
    }
    for (const char* folder : kEmptyFolders) {
      fs::create_directories(target / folder, error);
    }
    if (!CopyEngineResources(target / "GameProject")) {
      return "Could not copy the engine resources to " + ToUtf8(target / "GameProject" / "EngineResources");
    }

    // エンジンは写さず、動いているエンジンをジャンクションでつなぐ（管理者の権限が要らず、すぐ終わる）
    fs::create_directories(target / "Engine", error);
    int exitCode = -1;
    ChildProcess::Run(std::format(L"cmd.exe /c mklink /J \"{}\" \"{}\"", (target / "Engine" / "TakoEngine").wstring(), engineRoot.wstring()), &exitCode);
    if (exitCode != 0) {
      return "Could not link the engine to " + ToUtf8(target / "Engine" / "TakoEngine");
    }
    return {};
  }

  bool OpenInNewEditor(const fs::path& project)
  {
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    // 末尾が \ のパスでも閉じる " が効くように、. を足す
    std::wstring        command = std::format(L"\"{}\" --project \"{}\\.\"", executable, project.wstring());
    STARTUPINFOW        startup{ sizeof(startup) };
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &info)) {
      return false;
    }
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    return true;
  }

  fs::path BrowseForProject()
  {
    wchar_t       buffer[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrTitle  = L"Open Project";
    dialog.lpstrFilter = L"Tako Project (*.takoproject)\0*.takoproject\0";
    dialog.lpstrFile   = buffer;
    dialog.nMaxFile    = MAX_PATH;
    dialog.Flags       = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;
    return GetOpenFileNameW(&dialog) ? fs::path(buffer).parent_path() : fs::path();
  }

  int CompareVersions(std::string_view a, std::string_view b)
  {
    // 数字でない物（v や .）を読み飛ばして、次の数を取り出す。もう無ければ 0
    const auto next = [](std::string_view& text) {
      while (!text.empty() && !std::isdigit(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
      }
      int        value = 0;
      const auto end   = std::from_chars(text.data(), text.data() + text.size(), value).ptr;
      text.remove_prefix(static_cast<size_t>(end - text.data()));
      return value;
    };
    while (!a.empty() || !b.empty()) {
      const int left  = next(a);
      const int right = next(b);
      if (left != right) {
        return left < right ? -1 : 1;
      }
    }
    return 0;
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

  std::string GetEngineVersion()
  {
    std::ifstream file(GetEngineRoot() / "VERSION");
    std::string   version;
    std::getline(file, version);
    while (!version.empty() && std::isspace(static_cast<unsigned char>(version.back()))) {
      version.pop_back();
    }
    return version;
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

  std::vector<fs::path> GetRecentProjects()
  {
    std::vector<fs::path> projects;
    const json            list = ReadJson(RecentFile());
    if (list.is_array()) {
      for (const json& item : list) {
        if (item.is_string()) {
          projects.emplace_back(StringUtility::ConvertString(item.get<std::string>()));
        }
      }
    }
    return projects;
  }

} // namespace Tako::Project

#endif // _DEBUG
