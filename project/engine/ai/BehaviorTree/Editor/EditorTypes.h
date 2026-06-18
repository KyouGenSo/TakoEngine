#pragma once

#ifdef _DEBUG

#include <string>
#include <vector>
#include <imgui.h>
#include "BTNode.h"

namespace Tako {

/// <summary>
/// エディタノードデータ (エディタ専用メタデータ)。
/// imgui-node-editor 上の 1 ノードに紐づく情報を保持する。
/// </summary>
struct EditorNode {
  int              id;            ///< エディタ固有 ID (10000 番台)
  ImVec2           position;      ///< エディタキャンバス上の位置
  std::string      nodeType;
  std::string      displayName;   ///< 表示名 (パレット名、ユーザーが変更可能)
  BTNodePtr        runtimeNode;   ///< 実際の実行時ノード (ランタイムインスタンス)
  std::vector<int> inputPinIds;   ///< 入力ピン ID 群 (親接続用、通常 1 個)
  std::vector<int> outputPinIds;  ///< 出力ピン ID 群 (子接続用、コンポジットなら 1 個以上)
  ImVec4           color;         ///< ノード描画色 (BTNodeRegistry の NodeMeta から取得)
};

/// <summary>
/// エディタリンクデータ (ノード間の接続)。
/// </summary>
struct EditorLink {
  int id;           ///< リンク固有 ID (30000 番台)
  int startPinId;   ///< 開始ピン ID (出力ピン側)
  int endPinId;     ///< 終了ピン ID (入力ピン側)
  int startNodeId;  ///< 開始ノード ID
  int endNodeId;    ///< 終了ノード ID
};

/// <summary>
/// エディタピンデータ (ノードへの接続点)。
/// </summary>
struct EditorPin {
  int         id;       ///< ピン固有 ID (20000 番台)
  int         nodeId;   ///< 所属ノード ID
  bool        isInput;  ///< 入力ピン (true) か出力ピン (false) か
  std::string name;     ///< ピン名 (表示用)
};

} // namespace Tako

#endif // _DEBUG
