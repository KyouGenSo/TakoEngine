#ifdef _DEBUG

#include "ScriptCompiler.h"
#include "ChildProcess.h"
#include "DebugUIManager.h"
#include "Project.h"
#include "StringUtility.h"
#include "WinApp.h"

#include <Windows.h>
#include <algorithm>
#include <format>
#include <system_error>

namespace Tako {

  namespace {

    namespace fs = std::filesystem;

    using LogType = DebugUIManager::LogType;

    /// <summary>
    /// 一番新しい Visual Studio の MSBuild.exe。vswhere で最初の 1 回だけ探す
    /// </summary>
    /// <returns>見つからなければ空</returns>
    const std::wstring& FindMSBuild()
    {
      static const std::wstring path = [] {
        wchar_t programFiles[MAX_PATH]{};
        GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, MAX_PATH);
        const std::wstring command = std::format(
          L"\"{}\\Microsoft Visual Studio\\Installer\\vswhere.exe\" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe", programFiles);
        const std::string output = ChildProcess::Run(command);
        // 複数見つかると 1 行に 1 つずつ出る
        const std::string first = output.substr(0, output.find_first_of("\r\n"));
        return fs::exists(first) ? StringUtility::ConvertString(first) : std::wstring();
      }();
      return path;
    }

    /// <summary>
    /// Script と Source の .h / .cpp のうち、一番新しい更新時刻
    /// </summary>
    fs::file_time_type NewestSourceTime()
    {
      fs::file_time_type newest = (fs::file_time_type::min)();
      for (const char* root : { "Script", "Source" }) {
        std::error_code error;
        for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
          const fs::path extension = it->path().extension();
          if ((extension == ".h" || extension == ".cpp") && it->is_regular_file(error)) {
            newest = (std::max)(newest, it->last_write_time(error));
          }
        }
      }
      return newest;
    }

    /// <summary>
    /// MSBuild が書いた 1 行を UTF-8 にする。Visual Studio 18 の MSBuild は UTF-8 で書くので、UTF-8 として読めない行だけ、コンソールのコードページから変換する
    /// </summary>
    std::string ConsoleToUtf8(const std::string& line)
    {
      if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), nullptr, 0) > 0) {
        return line;
      }
      const int    length = MultiByteToWideChar(CP_OEMCP, 0, line.data(), static_cast<int>(line.size()), nullptr, 0);
      std::wstring wide(static_cast<size_t>(length), L'\0');
      MultiByteToWideChar(CP_OEMCP, 0, line.data(), static_cast<int>(line.size()), wide.data(), length);
      return StringUtility::ConvertString(wide);
    }

  } // namespace

  ScriptCompiler::~ScriptCompiler()
  {
    // ビルドの途中で閉じても MSBuild は止めない（中間ファイルを壊さない）
    if (process_) {
      CloseHandle(process_);
      CloseHandle(outputRead_);
    }
  }

  void ScriptCompiler::Compile()
  {
    if (process_) {
      return;
    }
    attemptedSourceTime_ = NewestSourceTime();

    const fs::path project = Project::GetProjectFile();
    if (project.empty()) {
      LogToConsole("Compile: the working folder must contain exactly one .vcxproj", LogType::Error);
      return;
    }
    const std::wstring& msbuild = FindMSBuild();
    if (msbuild.empty()) {
      LogToConsole("Compile: MSBuild was not found. Install Visual Studio with the C++ workload", LogType::Error);
      return;
    }

    // nr:false: ビルドの後に MSBuild のプロセスを残さない（残ると、TakoHeaderTool の古い版を持ち続ける）
    const std::wstring command = std::format(L"\"{}\" \"{}\" /p:Configuration=Debug /p:Platform=x64 /nologo /v:m /nr:false", msbuild, fs::absolute(project).wstring());
    if (!ChildProcess::Spawn(command, process_, outputRead_)) {
      process_ = nullptr;
      LogToConsole(std::format("Compile: could not start MSBuild (error {})", GetLastError()), LogType::Error);
      return;
    }
    errorCount_ = 0;
    pendingOutput_.clear();
    currentStep_.clear();
    startTime_ = std::chrono::steady_clock::now();
    LogToConsole(std::format("Compile started: {}", StringUtility::ConvertString(project.filename().wstring())));
  }

  void ScriptCompiler::Update()
  {
    if (process_) {
      ReadOutput();
      if (WaitForSingleObject(process_, 0) == WAIT_OBJECT_0) {
        ReadOutput();
        Finish();
      }
      return;
    }

    if (!hasCheckedStartup_) {
      // 起動した時点で DLL より新しいソースがあれば（DLL が無いときも）、最初にフォーカスを持ったフレームでビルドする
      hasCheckedStartup_ = true;
      std::error_code error;
      attemptedSourceTime_ = fs::last_write_time(Project::GetGameModulePath(), error);
      if (error) {
        attemptedSourceTime_ = (fs::file_time_type::min)();
      }
    }

    // 保存のたびではなく、エディタに戻ってきたときにだけ見る（書きかけでビルドしない）
    const bool isFocused   = GetForegroundWindow() == WinApp::GetInstance()->GetHWnd();
    const bool hasGotFocus = isFocused && !wasFocused_;
    wasFocused_            = isFocused;
    if (isAutoCompile_ && hasGotFocus && NewestSourceTime() > attemptedSourceTime_) {
      Compile();
    }
  }

  void ScriptCompiler::ReadOutput()
  {
    ChildProcess::ReadAvailable(outputRead_, pendingOutput_);
    // 文字の途中で切らないよう、行がそろってから変換する
    size_t begin = 0;
    for (size_t end = pendingOutput_.find('\n'); end != std::string::npos; end = pendingOutput_.find('\n', begin)) {
      LogLine(ConsoleToUtf8(pendingOutput_.substr(begin, end - begin)));
      begin = end + 1;
    }
    pendingOutput_.erase(0, begin);
  }

  void ScriptCompiler::LogLine(const std::string& line)
  {
    const bool isError   = line.find(": error ") != std::string::npos || line.find(": fatal error ") != std::string::npos;
    const bool isWarning = line.find(": warning ") != std::string::npos;
    const size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      return;
    }
    if (!isError && !isWarning) {
      currentStep_ = line.substr(first, line.find_last_not_of(" \t\r\n") + 1 - first);
      return;
    }
    // 行の頭の空白と、末尾の [プロジェクトのパス] を落とす（ファイル(行) で始まる形にして、Console のダブルクリックで開けるようにする）
    size_t       last  = line.rfind(" [");
    if (last == std::string::npos || line.find(".vcxproj]", last) == std::string::npos) {
      last = line.find_last_not_of("\r\n") + 1;
    }
    errorCount_ += isError ? 1 : 0;
    LogToConsole(line.substr(first, last - first), isError ? LogType::Error : LogType::Warning);
  }

  void ScriptCompiler::Finish()
  {
    DWORD exitCode = 1;
    GetExitCodeProcess(process_, &exitCode);
    CloseHandle(process_);
    CloseHandle(outputRead_);
    process_    = nullptr;
    outputRead_ = nullptr;

    if (exitCode == 0) {
      LogToConsole(std::format("Compile succeeded ({:.1f} s)", GetElapsedSeconds()));
    }
    else {
      LogToConsole(std::format("Compile failed: {} error(s). The running scripts were not replaced", errorCount_), LogType::Error);
    }
  }

} // namespace Tako

#endif // _DEBUG
