#pragma once
#include <string>

namespace Tako {

  /// <summary>
  /// ノードのカテゴリ。エディタのパレット分類やシリアライズで使用する。
  /// </summary>
  enum class NodeCategory {
    /// <summary>
    /// コンポジットノード (子を持てる: Selector / Sequence / Parallel など)
    /// </summary>
    Composite,
    /// <summary>
    /// アクションノード (実際の処理)
    /// </summary>
    Action,
    /// <summary>
    /// 条件ノード (判定)
    /// </summary>
    Condition,
    /// <summary>
    /// デコレータノード (将来の拡張用)
    /// </summary>
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
    std::string  displayName;                         ///< 表示名 (パレット項目・ノードキャプション)
    NodeCategory category    = NodeCategory::Action;  ///< カテゴリ (パレット分類)
    NodeColor    color;                               ///< ノードカラー (エディタ表示色、RGBA)
    bool         isComposite = false;                 ///< コンポジット (子ノードを持てる) かどうか
  };

} // namespace Tako
