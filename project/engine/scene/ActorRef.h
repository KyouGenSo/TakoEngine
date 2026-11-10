#pragma once
#include <cstdint>

namespace Tako {

  using ActorId = uint64_t;

  /// <summary>
  /// 同じシーンの Actor を指す値。ID で保存するので、シーンを開き直しても同じ相手を指す。
  /// TPROPERTY にすると Inspector で選べる。相手は Actor::Resolve で取る
  /// </summary>
  struct ActorRef {
    ActorId id = 0;  ///< 0 は誰も指していない
  };

} // namespace Tako
