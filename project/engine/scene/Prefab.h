#pragma once
#include "ActorRef.h"

#include <json_fwd.hpp>

#include <functional>
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
  // 組から消した Actor の番号は、根の "removed" に持つ（プレハブに後から足された Actor と見分けるため）。
  // プレハブのファイルの中にも、同じ形で別のプレハブを置ける（入れ子）。読むときに内側から開くので、
  // シーンの Actor は外側のプレハブだけを指し、差分も外側の物として持つ。
  // 制限：置いた組の途中の Actor から別の名前で Save as Prefab すると、中のプレハブは写しになる

  /// <summary>
  /// ID の付け替えの表（元の ID → 新しい ID）
  /// </summary>
  using ActorIdMap = std::map<ActorId, ActorId>;

  /// <summary>
  /// プレハブとのつながり。prefab が空ならつながっていない
  /// </summary>
  struct PrefabLink {
    std::string prefab;
    ActorId     node = 0;  ///< プレハブの中の番号
    ActorId     root = 0;  ///< 組の根。自分が根なら 0
  };

  // 中に置いたプレハブに後から足された Actor の番号は、外側のファイルに無いので計算で決める。このビットが立つ（ファイルに書いた番号と重ならない）
  inline constexpr ActorId kImplicitPrefabNode = ActorId{ 1 } << 63;

  std::string GetPrefabPath(std::string_view name);

  bool HasPrefab(std::string_view name);

  std::vector<std::string> GetPrefabNames();

  /// <summary>
  /// プレハブの中の Actor の番号（先頭が根）。中に置いたプレハブの Actor も入る。プレハブが無ければ空
  /// </summary>
  std::vector<ActorId> GetPrefabNodes(std::string_view name);

  /// <summary>
  /// プレハブ name の node が、その中に置いた別のプレハブの物なら、そのつながり（root は name の中の番号）
  /// </summary>
  PrefabLink GetNestedPrefabLink(std::string_view name, ActorId node);

  /// <summary>
  /// Actor::Save に、プレハブとのつながり（prefab / node / root）を添えた 1 件
  /// </summary>
  nlohmann::json SaveActorEntry(Actor& actor);

  /// <summary>
  /// actor と子孫を { "actors": [actor, 子...] } にする（複製・貼り付け・プレハブの元）。
  /// シーンファイルと違い、プレハブの物も差分ではなく全部の値で書く
  /// </summary>
  nlohmann::json SaveActorTree(Actor& actor);

  /// <summary>
  /// Actor の JSON（1 件か配列）の id・parent・root と、中の ActorRef を表で付け替える。
  /// 根が表に無い Actor は、プレハブとのつながりを外す（組の一部だけを写した物は、同じ番号を名乗れない）
  /// </summary>
  /// <param name="keepsUnmapped">表に無い ID を、true なら残す（同じシーンの中の複製）、false なら外す（プレハブ・別のシーン）</param>
  void RemapActorIds(nlohmann::json& json, const ActorIdMap& ids, bool keepsUnmapped);

  /// <summary>
  /// 保存の形の "actors" を、全部の値の配列に開く（シーンファイルとプレハブのファイルの読み込み）。
  /// 置いた物（type が無く prefab を持つ物）はプレハブと差分から作り、prefab / node / root を残す。
  /// プレハブに後から足された Actor も足す（removed の番号と、その子孫は足さない）。プレハブや番号が無くなった物は、ログを出して落とす
  /// </summary>
  /// <param name="newId">足した Actor の ID を決める（組の根の ID, プレハブの中の番号）</param>
  nlohmann::json ExpandPlacedActors(nlohmann::json entries, const std::function<ActorId(ActorId root, ActorId node)>& newId);

  /// <summary>
  /// ExpandPlacedActors の逆。全部の値の配列（SaveActorEntry の形）を保存の形にする。
  /// プレハブの物は差分だけにし、組の根には、組に居ない番号を removed として添える
  /// </summary>
  nlohmann::json CollapsePlacedActors(const nlohmann::json& entries);

  /// <summary>
  /// SaveActorTree の内容を、プレハブ name のファイルの中身にする。組の外への ActorRef と根の親は落とす。
  /// 木の中に丸ごと入っている別のプレハブの組は、入れ子（置いた物）として書く。name をすでに置いた組から書くとき（Apply）は、今の入れ子を保つ
  /// </summary>
  /// <param name="nodes">シーンの ID → プレハブの中の番号（組の全部の Actor）</param>
  nlohmann::json MakePrefabContent(std::string_view name, const nlohmann::json& tree, const ActorIdMap& nodes);

  /// <summary>
  /// MakePrefabContent の内容をファイルに書く
  /// </summary>
  bool SavePrefab(std::string_view name, const nlohmann::json& content);

  /// <summary>
  /// プレハブの node の Actor を、Actor::Load 用の JSON にする。当てられない差分はログを出して飛ばす
  /// </summary>
  /// <param name="ids">プレハブの中の番号 → シーンの ID</param>
  /// <param name="overrides">置いた物の overrides（無ければ nullptr）</param>
  /// <returns>プレハブか node が無ければ null</returns>
  nlohmann::json ResolvePrefabNode(std::string_view name, ActorId node, const ActorIdMap& ids, const nlohmann::json* overrides);

#ifdef _DEBUG
  /// <summary>
  /// ファイルを書かずに、プレハブをメモリーの上だけに置く（自己テスト用）。content が null なら外す
  /// </summary>
  void SetPrefabInMemory(std::string_view name, const nlohmann::json& content);
#endif

} // namespace Tako
