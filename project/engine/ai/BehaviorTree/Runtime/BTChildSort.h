#pragma once
#include <algorithm>
#include <vector>

namespace Tako {

  /// <summary>
  /// 子ノードIDをキャンバス座標順に整列する。
  /// 広がりが大きい軸の昇順 (横並び=左→右 / 縦並び=上→下)、同値はもう一方の軸で比較。
  /// エディタ表示とランタイム構築の実行順を一致させる唯一の正とする。
  /// </summary>
  /// getPos: bool(int id, float& x, float& y)。false を返したノードは相対順維持 (stable)
  template <typename GetPosFn>
  void SortChildIdsByPosition(std::vector<int>& childIds, GetPosFn getPos) {
    if (childIds.size() < 2) return;

    bool first = true;
    float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
    for (int id : childIds) {
      float x = 0.0f, y = 0.0f;
      if (!getPos(id, x, y)) continue;
      if (first) {
        minX = maxX = x;
        minY = maxY = y;
        first = false;
      } else {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
      }
    }
    const bool horizontal = (maxX - minX) >= (maxY - minY);

    std::stable_sort(childIds.begin(), childIds.end(),
      [&getPos, horizontal](int a, int b) {
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        if (!getPos(a, ax, ay) || !getPos(b, bx, by)) return false;
        const float pa = horizontal ? ax : ay;
        const float pb = horizontal ? bx : by;
        if (pa != pb) return pa < pb;
        return (horizontal ? ay : ax) < (horizontal ? by : bx);
      });
  }

} // namespace Tako
