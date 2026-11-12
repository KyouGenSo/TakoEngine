#ifdef _DEBUG

#include "ChildProcess.h"

#include <Windows.h>
#include <algorithm>
#include <vector>

namespace Tako::ChildProcess {

  bool Spawn(std::wstring commandLine, void*& process, void*& outputRead)
  {
    SECURITY_ATTRIBUTES security{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE              outputWrite = nullptr;
    if (!CreatePipe(&outputRead, &outputWrite, &security, 0)) {
      return false;
    }
    // 読む側まで子に渡すと、子が持ち続けてパイプが閉じない
    SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);

    // 子に渡すハンドルを、書く側のパイプだけにする。全部渡すと、ビルドの後もしばらく残る mspdbsrv.exe が、そのとき開いていたファイルを掴み続ける
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<char>                  attributeBuffer(attributeSize);
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
    InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize);
    UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &outputWrite, sizeof(HANDLE), nullptr, nullptr);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb         = sizeof(startup);
    startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = outputWrite;
    startup.StartupInfo.hStdError  = outputWrite;
    startup.lpAttributeList        = attributes;
    PROCESS_INFORMATION info{};
    const BOOL          isStarted = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                                   &startup.StartupInfo, &info);
    DeleteProcThreadAttributeList(attributes);
    CloseHandle(outputWrite);
    if (!isStarted) {
      CloseHandle(outputRead);
      outputRead = nullptr;
      return false;
    }
    CloseHandle(info.hThread);
    process = info.hProcess;
    return true;
  }

  void ReadAvailable(void* outputRead, std::string& out)
  {
    char  buffer[4096];
    DWORD available = 0;
    while (PeekNamedPipe(outputRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
      DWORD size = 0;
      if (!ReadFile(outputRead, buffer, (std::min)(available, static_cast<DWORD>(sizeof(buffer))), &size, nullptr) || size == 0) {
        break;
      }
      out.append(buffer, size);
    }
  }

  std::string Run(std::wstring commandLine, int* exitCode)
  {
    if (exitCode) {
      *exitCode = -1;
    }
    void* process    = nullptr;
    void* outputRead = nullptr;
    if (!Spawn(std::move(commandLine), process, outputRead)) {
      return {};
    }
    // 子が終わって書く側が閉じると、ReadFile が失敗して抜ける
    std::string output;
    char        buffer[1024];
    DWORD       size = 0;
    while (ReadFile(outputRead, buffer, sizeof(buffer), &size, nullptr) && size > 0) {
      output.append(buffer, size);
    }
    WaitForSingleObject(process, INFINITE);
    DWORD code = 0;
    if (exitCode && GetExitCodeProcess(process, &code)) {
      *exitCode = static_cast<int>(code);
    }
    CloseHandle(outputRead);
    CloseHandle(process);
    return output;
  }

} // namespace Tako::ChildProcess

#endif // _DEBUG
