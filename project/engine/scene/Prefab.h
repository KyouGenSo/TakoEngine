#pragma once
#include "ActorRef.h"

#include <json_fwd.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  class Actor;

  // プレハブは Actor の組（根と子孫）を、小さいシーンと同じ形 { "actors": [根, 子...] } で書いた物（resources/Json/Prefabs/名前.prefab.json）。
  // ID はプレハブの中だけの番号。"actors" の無い古いファイルは、Actor 1 つのプレハブ（番号 1）として読む。
  // シーンに置いた物は、組の Actor を 1 件ずつ
  //   { "id", "prefab": 名前, "node": プレハブの中の番号, "root": 組の根の ID（根は書かない）, "overrides": プレハブとの差分（RFC 6902 の JSON Patch） }
  // で保存する。根の名前と Transform は、いつも差分として持つ（プレハブを書き換えても、置いた物が動かないように）。
  // 制限：プレハブの中に別のプレハブは入らない（写しになる）。置いた組の子を消しても、開き直すと戻る（消すなら Unpack）

  /// <summary>
  /// ID の付け替えの表（元の ID → 新しい ID）
  /// </summary>
  using ActorIdMap = std::map<ActorId, ActorId>;

  std::string GetPrefabPath(std::string_view name);

  bool HasPrefab(std::string_view name);

  std::vector<std::string> GetPrefabNames();

  /// <summary>
  /// プレハブの中の Actor の番号（先頭が根）。プレハブが無ければ空
  /// </summary>
  std::vector<ActorId> GetPrefabNodes(std::string_view name);

  /// <summary>
  /// actor と子孫を { "actors": [actor, 子...] } にする（複製・貼り付け・プレハブの元）。
  /// シーンファイルと違い、プレハブの物も差分ではなく全部の値で書き、プレハブとのつながり（prefab / node / root）を添える
  /// </summary>
  nlohmann::json SaveActorTree(Actor& actor);

  /// <summary>
  /// Actor の JSON（1 件か配列）の id・parent・root と、中の ActorRef を表で付け替える。
  /// 根が表に無い Actor は、プレハブとのつながりを外す（組の一部だけを写した物は、同じ番号を名乗れない）
  /// </summary>
  /// <param name="keepsUnmapped">表に無い ID を、true なら残す（同じシーンの中の複製）、false なら外す（プレハブ・別のシーン）</param>
  void RemapActorIds(nlohmann::json& json, const ActorIdMap& ids, bool keepsUnmapped);

  /// <summary>
  /// SaveActorTree の内容をプレハブのファイルの中身にする。組の外への ActorRef・根の親・他のプレハブとのつながりは落とす
  /// </summary>
  /// <param name="nodes">シーンの ID → プレハブの中の番号（組の全部の Actor）</param>
  nlohmann::json MakePrefabContent(const nlohmann::json& tree, const ActorIdMap& nodes);

  /// <summary>
  /// MakePrefabContent の内容をファイルに書く
  /// </summary>
  bool SavePrefab(std::string_view name, const nlohmann::json& content);

  /// <summary>
  /// シーンファイルに書く 1 件。プレハブの物はプレハブとの差分だけにする
  /// </summary>
  /// <param name="ids">actor が入っている組の、プレハブの中の番号 → シーンの ID</param>
  nlohmann::json SavePrefabInstance(Actor& actor, const ActorIdMap& ids);

  /// <summary>
  /// プレハブの node の Actor を、Actor::Load 用の JSON にする。当てられない差分はログを出して飛ばす
  /// </summary>
  /// <param name="ids">プレハブの中の番号 → シーンの ID</param>
  /// <param name="overrides">SavePrefabInstance の overrides（無ければ nullptr）</param>
  /// <returns>プレハブか node が無ければ null</returns>
  nlohmann::json ResolvePrefabNode(std::string_view name, ActorId node, const ActorIdMap& ids, const nlohmann::json* overrides);

#ifdef _DEBUG
  /// <summary>
  /// ファイルを書かずに、プレハブをメモリーの上だけに置く（自己テスト用）。content が null なら外す
  /// </summary>
  void SetPrefabInMemory(std::string_view name, const nlohmann::json& content);
#endif

} // namespace Tako
