#pragma once
#ifdef _DEBUG

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// <summary>
/// エディタが開いているゲームのプロジェクト。ルートの下に GameProject（作業フォルダー）・Engine・Generated がある
/// </summary>
namespace Tako::Project {

  /// <summary>
  /// コマンドラインを読んでプロジェクトを開き、作業フォルダーを GameProject に移す。
  ///   --project ルート : そのプロジェクトを開く
  ///   指定なし          : 作業フォルダーが GameProject ならそれを開く
  /// </summary>
  /// <returns>エディタを続けるなら空。終わるなら終了コード</returns>
  TAKO_API std::optional<int> Startup();

  //=====================================
  //Getter
  //=====================================
  TAKO_API const std::filesystem::path& GetRoot();

  /// <summary>
  /// エンジンのルート（bin・build・project・templates のあるフォルダー）
  /// </summary>
  TAKO_API std::filesystem::path GetEngineRoot();

  /// <summary>
  /// 個人設定の置き場所（%APPDATA%/TakoEngine）
  /// </summary>
  TAKO_API std::filesystem::path GetUserSettingsFolder();

  /// <summary>
  /// ビルドしたゲームの DLL の場所
  /// </summary>
  TAKO_API std::filesystem::path GetGameModulePath();

  /// <summary>
  /// ゲームのプロジェクトファイル（作業フォルダーにただ 1 つある .vcxproj）
  /// </summary>
  /// <returns>無いか複数あれば空</returns>
  TAKO_API std::filesystem::path GetProjectFile();

} // namespace Tako::Project

#endif // _DEBUG
