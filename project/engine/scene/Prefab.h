#pragma once
#include <json_fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Tako {

  class Actor;

  // プレハブは Actor 1 つ分の JSON（resources/Json/Prefabs/名前.prefab.json）。
  // シーンに置いた物は { "prefab": 名前, "overrides": プレハブとの差分（RFC 6902 の JSON Patch） } で保存する

  std::string GetPrefabPath(std::string_view name);

  bool HasPrefab(std::string_view name);

  std::vector<std::string> GetPrefabNames();

  /// <summary>
  /// actor をプレハブとして書く（ID は書かない）
  /// </summary>
  bool SavePrefab(std::string_view name, Actor& actor);

  /// <summary>
  /// シーンファイルに書く 1 件。プレハブから作った物はプレハブとの差分だけにする
  /// </summary>
  nlohmann::json SavePrefabInstance(Actor& actor);

  /// <summary>
  /// SavePrefabInstance の内容を、プレハブと合わせた Actor::Load 用の JSON にする（プレハブの物でなければそのまま）。
  /// プレハブが変わって当てられない差分はログを出して飛ばす
  /// </summary>
  nlohmann::json ResolvePrefabInstance(const nlohmann::json& json);

} // namespace Tako
