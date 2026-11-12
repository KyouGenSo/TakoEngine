#ifdef _DEBUG

#include "CrashHandler.h"
#include "Project.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <cwchar>
#include <filesystem>

#pragma comment(lib, "dbghelp.lib")

namespace Tako {

  namespace {

    constexpr const wchar_t* kMarkerName = L"crashed";

    // 落ちている最中はメモリを確保しないので、書く先は先に決めておく
    wchar_t crashFolder[MAX_PATH]{};

    std::filesystem::path Folder()
    {
      return Project::GetRoot() / "Generated" / "Crashes";
    }

    LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* exception)
    {
      SYSTEMTIME time{};
      GetLocalTime(&time);
      wchar_t path[MAX_PATH]{};
      swprintf_s(path, L"%s\\crash-%04d%02d%02d-%02d%02d%02d.dmp", crashFolder, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

      CreateDirectoryW(crashFolder, nullptr);
      const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info{ GetCurrentThreadId(), exception, FALSE };
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &info, nullptr, nullptr);
        CloseHandle(file);
      }

      // 次の起動に、落ちたことを伝える印（ConsumeCrashMarker が読む）
      wchar_t marker[MAX_PATH]{};
      swprintf_s(marker, L"%s\\%s", crashFolder, kMarkerName);
      const HANDLE markerFile = CreateFileW(marker, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (markerFile != INVALID_HANDLE_VALUE) {
        CloseHandle(markerFile);
      }
      // 書いた後は、今までどおり OS に任せて終わる
      return EXCEPTION_CONTINUE_SEARCH;
    }

  } // namespace

  void InstallCrashHandler()
  {
    // ponytail: 古いダンプは消していない。たまったら Generated/Crashes ごと消してよい
    wcsncpy_s(crashFolder, Folder().c_str(), _TRUNCATE);
    SetUnhandledExceptionFilter(WriteCrashDump);
  }

  bool ConsumeCrashMarker()
  {
    std::error_code error;
    return std::filesystem::remove(Folder() / kMarkerName, error);
  }

} // namespace Tako

#endif // _DEBUG
