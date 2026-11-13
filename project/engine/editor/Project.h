#pragma once
#ifdef _DEBUG

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// <summary>
/// エディタが開いているゲームのプロジェクト。ルートの下に GameProject（作業フォルダー）・Engine・Generated があり、
/// ルートの 名前.takoproject に、最後に開いたエンジンのバージョンと保存形式の番号を持つ
/// </summary>
namespace Tako::Project {

  // プロジェクトの保存形式の番号。保存の形を変えたら 1 つ上げ、ProjectMigration.cpp の表に、古い形から直す手順を足す
  inline constexpr int kFormatVersion = 1;

  /// <summary>
  /// コマンドラインを読んでプロジェクトを開き、作業フォルダーを GameProject に移す。
  ///   --project ルート : そのプロジェクトを開く（.takoproject のパスでもよい）
  ///   --new 場所 名前   : テンプレートからプロジェクトを作って終わる
  ///   指定なし          : 作業フォルダーが GameProject ならそれを開く。違えば、開くか作るかを聞く
  /// </summary>
  /// <returns>エディタを続けるなら空。終わるなら終了コード</returns>
  TAKO_API std::optional<int> Startup();

  /// <summary>
  /// エンジンの templates/Default から、location/name にプロジェクトを作る。エンジンは、動いているエンジンへのジャンクションでつなぐ
  /// （IsEngineInsideProject なら、つながずに写しを持たせる）。エンジンのフォルダーの中には作れない
  /// </summary>
  /// <param name="name">プロジェクトの名前（英数字と _。フォルダーとファイルの名前になる）</param>
  /// <returns>失敗の理由。成功なら空</returns>
  TAKO_API std::string Create(const std::string& name, const std::filesystem::path& location);

  /// <summary>
  /// root のプロジェクトを、別のエディタで開く（このエディタはそのまま残る）
  /// </summary>
  TAKO_API bool OpenInNewEditor(const std::filesystem::path& root);

  /// <summary>
  /// .takoproject を選ぶダイアログを出す
  /// </summary>
  /// <returns>選んだプロジェクトのルート。取り消したら空</returns>
  TAKO_API std::filesystem::path BrowseForProject();

  /// <summary>
  /// 「2.10.3」のようなバージョンを、数として比べる（先頭の v は無視する）
  /// </summary>
  /// <returns>a が新しければ正、同じなら 0、古ければ負</returns>
  TAKO_API int CompareVersions(std::string_view a, std::string_view b);

  //=====================================
  //Getter
  //=====================================
  TAKO_API const std::filesystem::path& GetRoot();

  /// <summary>
  /// エンジンのルート（bin・build・project・templates のあるフォルダー）
  /// </summary>
  TAKO_API std::filesystem::path GetEngineRoot();

  /// <summary>
  /// 動いているエンジンが、別のプロジェクトの中にある写し（UpdateEngine.bat が入れた物）なら true。
  /// このエンジンで作るプロジェクトは、エンジンへリンクせず、自分の写しを持つ
  /// </summary>
  TAKO_API bool IsEngineInsideProject();

  /// <summary>
  /// エンジンの VERSION ファイルの中身（例: 2.1.0）
  /// </summary>
  /// <returns>読めなければ空</returns>
  TAKO_API std::string GetEngineVersion();

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

  /// <summary>
  /// 最近開いたプロジェクトのルート（新しい順）
  /// </summary>
  TAKO_API std::vector<std::filesystem::path> GetRecentProjects();

} // namespace Tako::Project

#endif // _DEBUG
