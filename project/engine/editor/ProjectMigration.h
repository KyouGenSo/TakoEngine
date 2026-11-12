#pragma once
#ifdef _DEBUG

#include <filesystem>
#include <string>

/// <summary>
/// 古いエンジンで保存したプロジェクトのファイルを、今の形へ直す
/// </summary>
namespace Tako::ProjectMigration {

  /// <summary>
  /// root のプロジェクトの保存ファイルを、形式 fromVersion から Project::kFormatVersion まで順に直す。先に resources/Json を Generated/Backup へ写す
  /// </summary>
  /// <returns>失敗の理由。成功なら空</returns>
  TAKO_API std::string Run(const std::filesystem::path& root, int fromVersion);

} // namespace Tako::ProjectMigration

#endif // _DEBUG
