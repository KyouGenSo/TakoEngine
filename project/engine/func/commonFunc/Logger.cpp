#include "Logger.h"
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <vector>

namespace Logger
{
  void Log(const std::string& message)
  {
    OutputDebugStringA(message.c_str());
  }

  // フォーマット文字列と可変引数を受け取る新しい関数
  void Log(const char* format, ...)
  {
    va_list args;
    va_start(args, format);

    // 必要なサイズを計算（NUL終端文字を含む）
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = vsnprintf(nullptr, 0, format, argsCopy) + 1;
    va_end(argsCopy);

    // メモリを動的に割り当て
    std::vector<char> buffer(size);

    // 実際にフォーマット
    vsnprintf(buffer.data(), size, format, args);
    va_end(args);

    // 既存のログ関数を呼び出す
    Log(std::string(buffer.data()));
  }
}