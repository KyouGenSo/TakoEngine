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
  /// エディタ固有 ID (10000 番台)
  int id;
  /// エディタキャンバス上の位置
  ImVec2 position;
  /// ノードタイプ名
  std::string nodeType;
  /// 表示名 (パレット名、ユーザーが変更可能)
  std::string displayName;
  /// 実際の実行時ノード (ランタイムインスタンス)
  BTNodePtr runtimeNode;
  /// 入力ピン ID 群 (親接続用、通常 1 個)
  std::vector<int> inputPinIds;
  /// 出力ピン ID 群 (子接続用、コンポジットなら 1 個以上)
  std::vector<int> outputPinIds;
  /// ノード描画色 (BTNodeRegistry の NodeMeta から取得)
  ImVec4 color;
};

/// <summary>
/// エディタリンクデータ (ノード間の接続)。
/// </summary>
struct EditorLink {
  /// リンク固有 ID (30000 番台)
  int id;
  /// 開始ピン ID (出力ピン側)
  int startPinId;
  /// 終了ピン ID (入力ピン側)
  int endPinId;
  /// 開始ノード ID
  int startNodeId;
  /// 終了ノード ID
  int endNodeId;
};

/// <summary>
/// エディタピンデータ (ノードへの接続点)。
/// </summary>
struct EditorPin {
  /// ピン固有 ID (20000 番台)
  int id;
  /// 所属ノード ID
  int nodeId;
  /// 入力ピン (true) か出力ピン (false) か
  bool isInput;
  /// ピン名 (表示用)
  std::string name;
};

} // namespace Tako

#endif // _DEBUG
