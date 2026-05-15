#pragma once
#include <string>

namespace Tako {

  /// <summary>
  /// ノードのカテゴリ。エディタのパレット分類やシリアライズで使用する。
  /// </summary>
  enum class NodeCategory {
    /// コンポジットノード (子を持てる: Selector / Sequence / Parallel など)
    Composite,
    /// アクションノード (実際の処理)
    Action,
    /// 条件ノード (判定)
    Condition,
    /// デコレータノード (将来の拡張用)
    Decorator
  };

  /// <summary>
  /// ノードカラー (RGBA、各成分 0.0-1.0)。
  /// ImGui 依存を BT runtime に持ち込まないために独自型として定義。
  /// エディタ側で必要に応じて ImVec4 等に変換する。
  /// </summary>
  struct NodeColor {
    float r = 0.4f;
    float g = 0.4f;
    float b = 0.4f;
    float a = 1.0f;

    NodeColor() = default;
    NodeColor(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {}
  };

  /// <summary>
  /// ノードのメタ情報。エディタ表示・パレット表示で使用する。
  /// </summary>
  struct NodeMeta {
    /// 表示名 (パレット項目・ノードキャプション)
    std::string displayName;
    /// カテゴリ (パレット分類)
    NodeCategory category = NodeCategory::Action;
    /// ノードカラー (エディタ表示色、RGBA)
    NodeColor color;
    /// コンポジット (子ノードを持てる) かどうか
    bool isComposite = false;
  };

} // namespace Tako
