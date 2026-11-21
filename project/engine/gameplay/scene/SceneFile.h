#pragma once
#include "ActorRef.h"

#include <json_fwd.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// シーンは、Actor 1 件を 1 ファイルにして保存する（別々の Actor を足す・直すだけなら、git で競合しない）。
//   resources/Json/Scenes/名前.scene.json       { "format": 2 } だけ。シーンがある印
//   resources/Json/Scenes/名前.scene/ID.json    Actor 1 件（BaseScene::Save の "actors" の 1 件）
//   resources/Json/Scenes/名前.scene/order.txt  並び。1 行に ID 1 つ
// どの Actor が居るかは order.txt が決める。フォルダーにあっても order.txt に無いファイルは読まない
// （Release のリソースのコピーは、消したファイルを出力先に残す。読むと、消した Actor が戻る）。
// "actors" を持つ 名前.scene.json は、全部を 1 ファイルに書いた形として読む（2.1.4 までのシーンと、作ったばかりのシーン）
namespace Tako::SceneFile {

  /// <summary>
  /// シーンとプレハブのファイルに書く文字列。id と type を先頭に置き、数字や文字列だけの配列は 1 行にする
  /// </summary>
  TAKO_API std::string ToText(const nlohmann::json& json);

  /// <summary>
  /// order.txt の中身を ID の並びにする。数字でない行（マージの競合マーカーも）と、2 回目の ID は飛ばす
  /// </summary>
  TAKO_API std::vector<ActorId> ParseOrder(std::string_view text);

  /// <summary>
  /// folder の Actor を order.txt の並びで読む。ファイルの無い ID は飛ばし、読めないファイルはログを出して飛ばす
  /// </summary>
  TAKO_API nlohmann::json ReadActors(const std::filesystem::path& folder);

  /// <summary>
  /// actors を folder に書き、前の order.txt にあって今は無い Actor のファイルを消す
  /// </summary>
  /// <returns>書けなかったか、消すことになるファイルが読めない（直す前の物かもしれない）なら false</returns>
  TAKO_API bool WriteActors(const std::filesystem::path& folder, const nlohmann::json& actors);

  /// <summary>
  /// シーン sceneName の中身（{ "actors": [...] }）。名前.scene.json が読めなければ nlohmann の例外を投げる
  /// </summary>
  TAKO_API nlohmann::json Read(std::string_view sceneName);

  /// <summary>
  /// BaseScene::Save の内容を、シーン sceneName のファイルに書く
  /// </summary>
  /// <returns>書けなかったか、今の 名前.scene.json が読めない（空で開いたシーンで上書きしない）なら false</returns>
  TAKO_API bool Write(std::string_view sceneName, const nlohmann::json& scene);

} // namespace Tako::SceneFile
