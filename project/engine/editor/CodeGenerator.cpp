#include "CodeGenerator.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>

namespace Tako::CodeGenerator {

  namespace {
    bool IsIdentifierChar(char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    std::string ReadFile(const std::filesystem::path& path, std::ios::openmode mode) {
      std::ifstream ifs(path, mode);
      return { std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>() };
    }

    bool WriteFile(const std::filesystem::path& path, const std::string& text, std::ios::openmode mode) {
      std::ofstream ofs(path, mode);
      ofs << text;
      return static_cast<bool>(ofs);
    }

    // 生成ファイル共通の先頭部分（include と、宣言に付ける summary まで）
    std::string Preamble(std::string_view include, std::string_view summary) {
      return std::format(
        "// 自動生成（Engine Settings の Save で上書きされるので直接編集しない）\n"
        "#pragma once\n"
        "#include <{}>\n"
        "\n"
        "/// <summary>\n"
        "/// {}\n"
        "/// </summary>\n",
        include, summary);
    }

    bool WriteIfChanged(const std::filesystem::path& path, const std::string& text) {
      // テキストモードで読み書きして改行コードの違いを無視する。同じ内容で書くとインクルード先が全部再ビルドになるため
      if (ReadFile(path, std::ios::in) == text) {
        return true;
      }
      std::error_code ec;
      std::filesystem::create_directories(path.parent_path(), ec);
      return WriteFile(path, text, std::ios::out);
    }
  }

  bool IsIdentifier(std::string_view name) {
    return !name.empty() && !(name.front() >= '0' && name.front() <= '9') && std::ranges::all_of(name, IsIdentifierChar);
  }

  bool WriteEnumHeader(const std::filesystem::path& path, std::span<const std::string> names, std::string_view summary) {
    std::string text = Preamble("cstdint", summary) + std::format("enum class {} : uint32_t {{\n", path.stem().string());
    for (size_t i = 0; i < names.size(); ++i) {
      text += std::format("    {} = {},\n", names[i], i);
    }
    text += "};\n";
    return WriteIfChanged(path, text);
  }

  bool WriteNameHeader(const std::filesystem::path& path, std::span<const std::string> names, std::string_view summary) {
    std::string text = Preamble("string_view", summary) + std::format("namespace {} {{\n", path.stem().string());
    for (const std::string& name : names) {
      text += std::format("    inline constexpr std::string_view {0} = \"{0}\";\n", name);
    }
    text += "}\n";
    return WriteIfChanged(path, text);
  }

  std::string ReplaceUsages(std::string_view text, std::string_view scopeName, const RenameMap& renames) {
    const std::string prefix = std::string(scopeName) + "::";
    std::string       result;
    result.reserve(text.size());

    size_t pos = 0;
    for (size_t hit = text.find(prefix); hit != std::string_view::npos; hit = text.find(prefix, pos)) {
      const size_t nameBegin = hit + prefix.size();
      size_t       nameEnd   = nameBegin;
      while (nameEnd < text.size() && IsIdentifierChar(text[nameEnd])) {
        ++nameEnd;
      }
      result += text.substr(pos, nameBegin - pos);

      // 直前が識別子の文字なら、別の名前（MyScope:: など）の末尾に一致しただけ
      const bool             isScopeName = hit == 0 || !IsIdentifierChar(text[hit - 1]);
      const std::string_view name        = text.substr(nameBegin, nameEnd - nameBegin);
      const auto             it          = renames.find(name);
      result += isScopeName && it != renames.end() ? std::string_view(it->second) : name;
      pos = nameEnd;
    }
    result += text.substr(pos);
    return result;
  }

  std::vector<std::filesystem::path> RenameUsages(const std::filesystem::path& root, std::string_view scopeName, const RenameMap& renames) {
    std::vector<std::filesystem::path> changed;
    if (renames.empty()) {
      return changed;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
      const std::string extension = entry.path().extension().string();
      if (!entry.is_regular_file() || (extension != ".h" && extension != ".hpp" && extension != ".cpp")) {
        continue;
      }
      // 文字コードと改行をそのまま保つためバイナリで扱う
      const std::string text     = ReadFile(entry.path(), std::ios::binary);
      const std::string replaced = ReplaceUsages(text, scopeName, renames);
      if (replaced != text && WriteFile(entry.path(), replaced, std::ios::binary)) {
        changed.push_back(entry.path());
      }
    }
    return changed;
  }

} // namespace Tako::CodeGenerator
