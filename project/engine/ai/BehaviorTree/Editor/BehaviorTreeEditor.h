#pragma once

#ifdef _DEBUG

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <json.hpp>
#include <imgui_node_editor.h>

#include "BTNode.h"
#include "EditorTypes.h"

namespace ed = ax::NodeEditor;

namespace Tako {

/// <summary>
/// BehaviorTreeEditor の初期化設定。
/// </summary>
struct EditorConfig {
  /// BT 関連 JSON の配置ディレクトリ。存在しない場合は Initialize で自動生成。
  std::string btJsonDir = "resources/Json/BT/";

  /// 初期ロードするツリーのファイル名 (拡張子付き、btJsonDir 配下)。空なら初回ロードしない。
  /// 各ツリーには "_{treeName}_layout.json" の layout が自動でペアリングされる。
  std::string initialTreeFile = "";

  /// メインエディタウィンドウの ImGui ウィンドウ名 (imgui.ini 位置/サイズの保存キー)。
  std::string windowName = "Behavior Tree Editor";

  /// ノードインスペクタウィンドウの ImGui ウィンドウ名。
  std::string nodeInspectorName = "Node Inspector##BTE";

  /// imgui-node-editor の Canvas 名 (SettingsFile 互換のため変更可)。
  std::string canvasName = "Behavior Tree Editor Canvas";
};

/// <summary>
/// ビヘイビアツリー用の汎用ノードエディタ。
/// imgui-node-editor を直接使用してビヘイビアツリーを視覚的に編集する。
/// ノード型は BTNodeRegistry に事前登録する必要がある。
/// </summary>
class BehaviorTreeEditor {
public:
  /// <summary>
  /// コンストラクタ。Initialize() を別途呼ぶこと。
  /// </summary>
  BehaviorTreeEditor() = default;

  /// <summary>
  /// デストラクタ。EditorContext が残っていれば Finalize しておくこと。
  /// </summary>
  ~BehaviorTreeEditor() = default;

  BehaviorTreeEditor(const BehaviorTreeEditor&) = delete;
  BehaviorTreeEditor& operator=(const BehaviorTreeEditor&) = delete;

  /// <summary>
  /// エディタの初期化。
  /// btJsonDir のディレクトリを自動生成し、imgui-node-editor のコンテキストを作る。
  /// initialTreeFile が指定されていれば LoadFromJSON で読み込む。
  /// </summary>
  /// <param name="config">エディタ設定</param>
  void Initialize(const EditorConfig& config);

  /// <summary>
  /// 1 フレーム分の更新・描画 。
  /// </summary>
  void Update();

  /// <summary>
  /// エディタの終了処理。imgui-node-editor のコンテキストを破棄する。
  /// </summary>
  void Finalize();

  /// <summary>
  /// 表示状態の切り替え。
  /// </summary>
  /// <param name="visible">表示するなら true</param>
  void SetVisible(bool visible) { isVisible_ = visible; }

  /// <summary>
  /// 表示状態の取得。
  /// </summary>
  /// <returns>表示中なら true</returns>
  bool IsVisible() const { return isVisible_; }

  /// <summary>
  /// JSON ファイルからツリーを読み込み (ノード・リンク復元)。
  /// </summary>
  /// <param name="filepath">読み込み元 JSON ファイルパス</param>
  /// <returns>成功すれば true</returns>
  bool LoadFromJSON(const std::string& filepath);

  /// <summary>
  /// JSON ファイルにツリーを保存。
  /// 親ディレクトリは std::filesystem::create_directories で自動生成。
  /// </summary>
  /// <param name="filepath">保存先 JSON ファイルパス</param>
  /// <returns>成功すれば true</returns>
  bool SaveToJSON(const std::string& filepath);

  //--- マルチツリー API (treeName ベース、内部で {btJsonDir}/{treeName}.json に解決) ---

  /// <summary>
  /// 指定ツリーを読み込み。currentTreeName_ を更新し未保存フラグをクリア。
  /// </summary>
  /// <param name="treeName">ツリー名 (拡張子なし、例: "MainTree")</param>
  /// <returns>成功すれば true</returns>
  bool LoadTree(const std::string& treeName);

  /// <summary>
  /// 現在のツリー状態を指定名で保存。
  /// </summary>
  /// <param name="treeName">保存先ツリー名 (拡張子なし)</param>
  /// <returns>成功すれば true</returns>
  bool SaveTree(const std::string& treeName);

  /// <summary>
  /// 新規空ツリーを作成 (既存ファイルがあれば失敗)。
  /// </summary>
  /// <param name="treeName">新規ツリー名</param>
  /// <returns>成功すれば true (重複名なら false)</returns>
  bool CreateNewTree(const std::string& treeName);

  /// <summary>
  /// 指定ツリーのファイルを削除 (現在編集中のツリーは削除不可)。
  /// </summary>
  /// <param name="treeName">削除対象ツリー名</param>
  /// <returns>成功すれば true</returns>
  bool DeleteTree(const std::string& treeName);

  /// <summary>
  /// 別ツリーに切替。未保存変更があれば確認モーダルを表示し、保存/破棄/キャンセルをユーザー選択。
  /// </summary>
  /// <param name="treeName">切替先ツリー名</param>
  void SwitchTree(const std::string& treeName);

  /// <summary>
  /// btJsonDir 内の利用可能ツリー一覧を取得 (アンダースコア prefix のファイルは除外)。
  /// </summary>
  /// <returns>ツリー名のリスト (sort 済み、拡張子なし)</returns>
  std::vector<std::string> ListAvailableTrees() const;

  /// <summary>
  /// 現在編集中のツリー名を取得。
  /// </summary>
  /// <returns>ツリー名 (空ならデフォルト)</returns>
  const std::string& GetCurrentTreeName() const { return currentTreeName_; }

  /// <summary>
  /// 未保存変更があるかチェック。
  /// </summary>
  /// <returns>未保存変更があれば true</returns>
  bool HasUnsavedChanges() const { return hasUnsavedChanges_; }

  /// <summary>
  /// 実行時ツリーを構築 (BehaviorTree::SetRootNode に渡す用)。
  /// ルートノードから再帰的に runtimeNode を組み立てる。
  /// </summary>
  /// <returns>ルートランタイムノード (ノード無しなら nullptr)</returns>
  BTNodePtr BuildRuntimeTree();

  /// <summary>
  /// 現在実行中のノードをエディタ上でハイライト表示。
  /// </summary>
  /// <param name="nodePtr">実行中のランタイムノード (nullptr ならクリア)</param>
  void HighlightRunningNode(const BTNodePtr& nodePtr);

  /// <summary>
  /// エディタの全データクリア (ノード・リンク・ピンを破棄、ID もリセット)。
  /// </summary>
  void Clear();

private:
  //--- 描画系 ---
  /// 全ノードを描画
  void DrawNodes();
  /// 単一ノードの描画 (ピン・タイトル・実行中ハイライト含む)
  void DrawNode(const EditorNode& node);
  /// 全リンクを描画
  void DrawLinks();
  /// 単一ピンの描画
  void DrawPin(const EditorPin& pin);
  /// ノード作成 UI を含むツールバー描画
  void DrawToolbar();
  /// 右クリックコンテキストメニュー描画
  void DrawContextMenu();
  /// 選択ノードのインスペクター描画 (パラメータ編集)
  void DrawNodeInspector();

  //--- インタラクション処理 ---
  /// リンク作成イベント処理 (循環参照チェック含む)
  void HandleLinkCreation();
  /// ノード/リンク削除イベント処理
  void HandleDeletion();

  //--- ノード操作 ---
  /// ノード生成 (自動 ID 割り当て)
  void CreateNode(const std::string& nodeType, const ImVec2& position);
  /// 指定 ID でノード生成 (JSON ロード復元用)
  int CreateNodeWithId(int nodeId, const std::string& nodeType, const ImVec2& position);
  /// リンク作成ヘルパー
  bool CreateLink(int sourceNodeId, int targetNodeId);

  //--- 検索ヘルパー ---
  EditorNode* FindNodeById(int nodeId);
  const EditorNode* FindNodeById(int nodeId) const;
  EditorNode* FindNodeByRuntimeNode(const BTNodePtr& node);
  EditorPin* FindPinById(int pinId);
  const EditorPin* FindPinById(int pinId) const;
  EditorLink* FindLinkById(int linkId);

  //--- ツリー構築・解析 ---
  /// 入力リンクを持たないノード (ルート) を探す
  int FindRootNodeId() const;
  /// 再帰的にランタイムツリーを構築
  void BuildRuntimeTreeRecursive(int nodeId, BTNodePtr& outNode);
  /// 循環参照チェック (BFS)
  bool HasCyclicDependency(int startNodeId, int endNodeId) const;
  /// 指定ノードの子ノード ID 一覧取得
  std::vector<int> GetChildNodeIds(int parentNodeId) const;

  //--- パラメータ保存/復元 ---
  /// ノードのパラメータを JSON に抽出
  nlohmann::json ExtractNodeParameters(const EditorNode& node);
  /// JSON からノードのパラメータを適用
  void ApplyNodeParameters(EditorNode& node, const nlohmann::json& params);

  //--- マルチツリー: ファイルパス組み立てヘルパー ---

  /// <summary>
  /// ツリー本体ファイルのフルパスを組み立て: "{btJsonDir}/{treeName}.json"
  /// </summary>
  std::string GetTreeFilePath(const std::string& treeName) const;

  /// <summary>
  /// ツリー対応 layout ファイル (imgui-node-editor SettingsFile) のフルパス:
  /// "{btJsonDir}/_{treeName}_layout.json"。アンダースコア prefix により
  /// ListAvailableTrees からは自動除外される。
  /// </summary>
  std::string GetLayoutFilePath(const std::string& treeName) const;

  /// <summary>
  /// imgui-node-editor の EditorContext を破棄して新しい SettingsFile (currentTreeName_ 連動)
  /// で再作成。ツリー切替時にレイアウトファイルをペアで切り替えるために呼ぶ。
  /// </summary>
  void RebuildEditorContext();

private:
  // 注入された初期化設定
  EditorConfig config_;

  // imgui-node-editor のエディタコンテキスト
  ed::EditorContext* editorContext_ = nullptr;
  // imgui-node-editor の設定 (SettingsFile 等)
  std::unique_ptr<ed::Config> editorConfig_;

  // エディタ上のノード一覧
  std::vector<EditorNode> nodes_;
  // エディタ上のリンク一覧
  std::vector<EditorLink> links_;
  // エディタ上のピン一覧
  std::vector<EditorPin> pins_;

  // ID カウンタ (ノード: 10000 番台で他エディタと分離)
  int nextNodeId_ = 10000;
  // ID カウンタ (リンク: 30000 番台)
  int nextLinkId_ = 30000;
  // ID カウンタ (ピン: 20000 番台)
  int nextPinId_ = 20000;

  // 表示状態
  bool isVisible_ = false;
  // 初回フレームフラグ (ed::SetNodePosition でノード位置を反映する 1 フレーム限定スイッチ)。
  // LoadFromJSON や CreateNode 後にも true に戻す。
  bool firstFrame_ = true;

  // 次の ed::End 前に ed::NavigateToContent() を呼んでビューを全ノードに合わせるフラグ。
  // 初回起動・LoadFromJSON 後に true。ナビゲート完了後 false に戻す。
  bool pendingNavigateToContent_ = true;
  // ハイライト中のノード ID (-1 ならハイライトなし)
  int highlightedNodeId_ = -1;
  // ハイライト開始時刻
  float highlightStartTime_ = 0.0f;
  // 現在選択中のノード ID (インスペクター連動)
  int selectedNodeId_ = -1;

  // ランタイムノードからエディタ ID への逆引きマップ
  std::unordered_map<BTNode*, int> runtimeNodeToEditorId_;

  // 次フレームで ed::SetNodePosition を呼びたいノード位置の予約リスト。
  // Add Node 直後の新規ノードのみ追加し、適用後クリアする。
  // LoadFromJSON では使わない (SettingsFile からの復元を優先する)。
  std::vector<std::pair<int, ImVec2>> pendingNodePositions_;

  //--- マルチツリー管理状態 ---

  // 現在編集中のツリー名 (拡張子なし、例: "MainTree")
  std::string currentTreeName_;

  // 未保存変更フラグ (CreateNode/DeleteNode/CreateLink 等で true、Save/Load で false)
  bool hasUnsavedChanges_ = false;

  // SwitchTree で未保存変更検出時に表示する確認モーダルのトリガー
  bool showUnsavedChangesModal_ = false;

  // SwitchTree 未保存確認モーダル経由で切替予定のツリー名
  std::string pendingSwitchTarget_;

  // 次回 Update 冒頭で ed::EditorContext を再作成するフラグ。
  // SettingsFile (= _{treeName}_layout.json) をツリーに合わせて切り替えるため、
  // LoadTree/SwitchTree 直後に立てる。
  bool pendingRebuildEditorContext_ = false;
};

} // namespace Tako

#endif // _DEBUG
