#include "Logger.h"
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <utility>
#include <vector>

namespace Tako {
  namespace Logger
  {
    // 内部ヘルパー関数（名前空間によって隠蔽）
    namespace detail {
      std::string FormatString(const char* format, va_list args) {
        va_list argsCopy;
        va_copy(argsCopy, args);
        int size = vsnprintf(nullptr, 0, format, argsCopy) + 1;
        va_end(argsCopy);

        std::vector<char> buffer(size);
        vsnprintf(buffer.data(), size, format, args);

        return std::string(buffer.data());
      }

      Sink sink = nullptr;
    }

    void Log(const std::string& message)
    {
      OutputDebugStringA(message.c_str());
      if (detail::sink) {
        detail::sink(message);
      }
    }

    Sink SetSink(Sink sink)
    {
      return std::exchange(detail::sink, sink);
    }

    void Log(const char* format, ...)
    {
      va_list args;
      va_start(args, format);

      std::string formattedMessage = detail::FormatString(format, args);
      va_end(args);

      Log(formattedMessage + "\n");
    }
  }
} // namespace Tako