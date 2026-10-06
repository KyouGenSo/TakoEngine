#pragma once

#ifdef _DEBUG

#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  /// <summary>
  /// 名前の一覧からヘッダ（enum / 名前の定数）を書き出し、改名に合わせてソース中の使用箇所を書き換える（エディタ用）
  /// </summary>
  namespace CodeGenerator {
    using RenameMap = std::map<std::string, std::string, std::less<>>;  ///< 旧名 → 新名

    /// <summary>
    /// C++ の識別子として使えるか（英字か _ で始まり、英数字と _ だけ）
    /// </summary>
    bool IsIdentifier(std::string_view name);

    /// <summary>
    /// names を 0 からの連番にした enum class（名前はファイル名、基底型 uint32_t）のヘッダを書き出す。内容が同じなら書かない
    /// </summary>
    /// <param name="summary">enum に付ける説明</param>
    /// <returns>書き込めた、または変更が無ければ true</returns>
    bool WriteEnumHeader(const std::filesystem::path& path, std::span<const std::string> names, std::string_view summary);

    /// <summary>
    /// names を同名の文字列定数（std::string_view）にした namespace（名前はファイル名）のヘッダを書き出す。内容が同じなら書かない
    /// </summary>
    /// <param name="summary">namespace に付ける説明</param>
    /// <returns>書き込めた、または変更が無ければ true</returns>
    bool WriteNameHeader(const std::filesystem::path& path, std::span<const std::string> names, std::string_view summary);

    /// <summary>
    /// text 中の「scopeName::旧名」を新名に置き換える。1 回の走査で置き換えるので名前の入れ替え（A↔B）も扱える
    /// </summary>
    std::string ReplaceUsages(std::string_view text, std::string_view scopeName, const RenameMap& renames);

    /// <summary>
    /// root 以下の .h / .hpp / .cpp に ReplaceUsages を適用し、変わったファイルだけ書き戻す（filesystem_error を投げうる）
    /// </summary>
    /// <returns>書き換えたファイル</returns>
    std::vector<std::filesystem::path> RenameUsages(const std::filesystem::path& root, std::string_view scopeName, const RenameMap& renames);
  }

} // namespace Tako

#endif // _DEBUG
