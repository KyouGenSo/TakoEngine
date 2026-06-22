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
  std::string btJsonDir         = "resources/Json/BT/";           ///< BT 関連 JSON の配置ディレクトリ。存在しない場合は Initialize で自動生成。
  std::string initialTreeFile   = "";                             ///< 初期ロードするツリーのファイル名 (拡張子付き、btJsonDir 配下)。空なら初回ロードしない。
  std::string windowName        = "Behavior Tree Editor";         ///< メインエディタウィンドウの ImGui ウィンドウ名 (imgui.ini 位置/サイズの保存キー)。
  std::string nodeInspectorName = "Node Inspector##BTE";          ///< ノードインスペクタウィンドウの ImGui ウィンドウ名。
  std::string canvasName        = "Behavior Tree Editor Canvas";  ///< imgui-node-editor の Canvas 名。
};

/// <summary>
/// ビヘイビアツリー用の汎用ノードエディタ。
/// imgui-node-editor を直接使用してビヘイビアツリーを視覚的に編集する。
/// ノード型は BTNodeRegistry に事前登録する必要がある。
/// </summary>
class BehaviorTreeEditor {
public: //メンバー関数
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
  /// 既存ツリーを複製して新規ツリーを作成する。
  /// 本体 json を複製し、複製後そのツリーへ切り替える。
  /// </summary>
  /// <param name="sourceTreeName">複製元ツリー名 (拡張子なし)</param>
  /// <param name="newTreeName">新規ツリー名 (拡張子なし)</param>
  /// <returns>成功すれば true</returns>
  bool CreateTreeFromCopy(const std::string& sourceTreeName, const std::string& newTreeName);

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

  //=========================================
  //Setter
  //=========================================
  void SetVisible(bool visible) { isVisible_ = visible; }

  //=========================================
  //Getter
  //=========================================
  bool IsVisible() const { return isVisible_; }
  const std::string& GetCurrentTreeName() const { return currentTreeName_; }
  bool HasUnsavedChanges() const { return hasUnsavedChanges_; }

private: //非公開関数
  //--- 描画系 ---
  void DrawNodes();
  /// <summary>
  /// 単一ノードの描画 (ピン・タイトル・実行中ハイライト含む)
  /// </summary>
  void DrawNode(const EditorNode& node);
  void DrawLinks();
  void DrawPin(const EditorPin& pin);
  /// <summary>
  /// ノード作成 UI を含むツールバー描画
  /// </summary>
  void DrawToolbar();
  /// <summary>
  /// 右クリックコンテキストメニュー描画
  /// </summary>
  void DrawContextMenu();
  /// <summary>
  /// 選択ノードのインスペクター描画 (パラメータ編集)
  /// </summary>
  void DrawNodeInspector();

  //--- インタラクション処理 ---
  /// <summary>
  /// リンク作成イベント処理 (循環参照チェック含む)
  /// </summary>
  void HandleLinkCreation();
  /// <summary>
  /// ノード/リンク削除イベント処理
  /// </summary>
  void HandleDeletion();

  //--- ノード操作 ---
  /// <summary>
  /// ノード生成 (自動 ID 割り当て)
  /// </summary>
  void CreateNode(const std::string& nodeType, const ImVec2& position);
  /// <summary>
  /// 指定 ID でノード生成 (JSON ロード復元用)
  /// </summary>
  int CreateNodeWithId(int nodeId, const std::string& nodeType, const ImVec2& position);
  /// <summary>
  /// リンク作成ヘルパー
  /// </summary>
  bool CreateLink(int sourceNodeId, int targetNodeId);

  //--- 検索ヘルパー ---
  EditorNode* FindNodeById(int nodeId);
  const EditorNode* FindNodeById(int nodeId) const;
  EditorNode* FindNodeByRuntimeNode(const BTNodePtr& node);
  EditorPin* FindPinById(int pinId);
  const EditorPin* FindPinById(int pinId) const;

  //--- ツリー構築・解析 ---
  /// <summary>
  /// 入力リンクを持たないノード (ルート) を探す
  /// </summary>
  int FindRootNodeId() const;
  /// <summary>
  /// 再帰的にランタイムツリーを構築
  /// </summary>
  void BuildRuntimeTreeRecursive(int nodeId, BTNodePtr& outNode);
  /// <summary>
  /// 循環参照チェック (BFS)
  /// </summary>
  bool HasCyclicDependency(int startNodeId, int endNodeId) const;
  /// <summary>
  /// 指定ノードの子ノード ID 一覧取得
  /// </summary>
  std::vector<int> GetChildNodeIds(int parentNodeId) const;
  /// <summary>
  /// 親内での実行順 (1 始まり)。ルートは -1
  /// </summary>
  int GetChildOrder(int nodeId) const;

  //--- パラメータ保存/復元 ---
  /// <summary>
  /// ノードのパラメータを JSON に抽出
  /// </summary>
  nlohmann::json ExtractNodeParameters(const EditorNode& node);
  /// <summary>
  /// JSON からノードのパラメータを適用
  /// </summary>
  void ApplyNodeParameters(EditorNode& node, const nlohmann::json& params);

  //--- マルチツリー: ファイルパス組み立てヘルパー ---

  /// <summary>
  /// ツリー本体ファイルのフルパスを組み立て: "{btJsonDir}/{treeName}.json"
  /// </summary>
  std::string GetTreeFilePath(const std::string& treeName) const;

private: //メンバー変数
  EditorConfig config_;  ///< 注入された初期化設定

  ed::EditorContext*          editorContext_ = nullptr;  ///< imgui-node-editor のエディタコンテキスト
  std::unique_ptr<ed::Config> editorConfig_;             ///< imgui-node-editor の設定
  std::vector<EditorNode> nodes_;  ///< エディタ上のノード一覧
  std::vector<EditorLink> links_;  ///< エディタ上のリンク一覧
  std::vector<EditorPin>  pins_;   ///< エディタ上のピン一覧

  int nextNodeId_ = 10000;  ///< ID カウンタ (ノード: 10000 番台で他エディタと分離)
  int nextLinkId_ = 30000;  ///< ID カウンタ (リンク: 30000 番台)
  int nextPinId_  = 20000;  ///< ID カウンタ (ピン: 20000 番台)

  bool isVisible_ = false;

  bool  pendingNavigateToContent_ = true;  ///< 次の ed::End 前に ed::NavigateToContent() を呼んでビューを全ノードに合わせるフラグ。初回起動・LoadFromJSON 後に true。ナビゲート完了後 false に戻す。
  int   highlightedNodeId_        = -1;    ///< ハイライト中のノード ID (-1 ならハイライトなし)
  float highlightStartTime_       = 0.0f;
  int   selectedNodeId_           = -1;    ///< 現在選択中のノード ID (インスペクター連動)

  std::unordered_map<BTNode*, int> runtimeNodeToEditorId_;  ///< ランタイムノードからエディタ ID への逆引きマップ

  std::vector<std::pair<int, ImVec2>> pendingNodePositions_;  ///< 次フレームの ed::Begin 内で ed::SetNodePosition を呼ぶノード位置の予約リスト。

  //--- マルチツリー管理状態 ---

  std::string currentTreeName_;  ///< 現在編集中のツリー名 (拡張子なし、例: "MainTree")

  bool hasUnsavedChanges_ = false;  ///< 未保存変更フラグ (CreateNode/DeleteNode/CreateLink 等で true、Save/Load で false)

  bool showUnsavedChangesModal_ = false;  ///< SwitchTree で未保存変更検出時に表示する確認モーダルのトリガー

  std::string pendingSwitchTarget_;  ///< SwitchTree 未保存確認モーダル経由で切替予定のツリー名
};

} // namespace Tako

#endif // _DEBUG
