#pragma once

#ifdef _DEBUG

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace Tako {

/**
 * @brief ノードエディタ管理クラス
 * imgui-node-editorを使用したビジュアルノードエディタシステムを管理
 */
class NodeEditorManager {
public:
    // ノード構造体
    struct Node {
        int id;
        std::string name;
        float posX, posY;
        std::vector<int> inputPins;
        std::vector<int> outputPins;
    };

    // ピン（接続点）構造体
    struct Pin {
        int id;
        int nodeId;
        bool isInput;
        std::string name;
        std::string type;
    };

    // リンク（接続）構造体
    struct Link {
        int id;
        int startPinId;
        int endPinId;
    };

private:
    static std::unique_ptr<NodeEditorManager> instance_;
    ed::EditorContext* context_ = nullptr;
    std::unique_ptr<ed::Config> config_;

    // ノードエディタのデータ
    std::vector<Node> nodes_;
    std::vector<Pin> pins_;
    std::vector<Link> links_;
    // IDカウンター（範囲を分離して競合を防ぐ）
    int nextNodeId_ = 1000;  // ノード: 1000番台
    int nextPinId_ = 2000;   // ピン: 2000番台
    int nextLinkId_ = 3000;  // リンク: 3000番台

    // エディタの状態
    bool isVisible_ = false;
    bool showGrid_ = true;
    bool autoArrange_ = false;
    bool firstFrame_ = true;  // 初回フレームフラグ（ノード位置設定用）

    // プライベートコンストラクタ/デストラクタ（シングルトン）
    NodeEditorManager() = default;
    ~NodeEditorManager();

    friend struct std::default_delete<NodeEditorManager>;

    // ノード描画関数
    void DrawNodes();
    void DrawLinks();
    void HandleCreation();
    void HandleDeletion();
    void DrawNode(const Node& node);
    void DrawPin(const Pin& pin);

    // ユーティリティ関数
    void AutoArrangeNodes();

    // ヘルパー関数（直接検索）
    Node* FindNode(int nodeId);
    Pin* FindPin(int pinId);
    Link* FindLink(int linkId);

public:
    NodeEditorManager(const NodeEditorManager&) = delete;
    NodeEditorManager& operator=(const NodeEditorManager&) = delete;

    /**
     * @brief シングルトンインスタンスの取得
     * @return NodeEditorManagerのインスタンス
     */
    static NodeEditorManager* GetInstance() {
        if (!instance_) {
            instance_ = std::unique_ptr<NodeEditorManager>(new NodeEditorManager());
        }
        return instance_.get();
    }

    /**
     * @brief マネージャーの初期化
     */
    void Initialize();

    /**
     * @brief マネージャーの終了処理
     */
    void Finalize();

    /**
     * @brief フレーム更新
     */
    void Update();

    /**
     * @brief ノードエディタの描画
     */
    void Draw();

    /**
     * @brief ウィンドウの表示/非表示切り替え
     */
    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }

    /**
     * @brief 新しいノードの作成
     * @param name ノード名
     * @param x X座標
     * @param y Y座標
     * @return 作成されたノードのID
     */
    int CreateNode(const std::string& name, float x = 0.0f, float y = 0.0f);

    /**
     * @brief ピンの追加
     * @param nodeId ノードID
     * @param name ピン名
     * @param isInput 入力ピンかどうか
     * @param type ピンのタイプ
     * @return 作成されたピンのID
     */
    int AddPin(int nodeId, const std::string& name, bool isInput, const std::string& type = "default");

    /**
     * @brief リンクの作成
     * @param startPinId 開始ピンID
     * @param endPinId 終了ピンID
     * @return 作成されたリンクのID（失敗時は-1）
     */
    int CreateLink(int startPinId, int endPinId);

    /**
     * @brief ノードの削除
     * @param nodeId ノードID
     */
    void DeleteNode(int nodeId);

    /**
     * @brief リンクの削除
     * @param linkId リンクID
     */
    void DeleteLink(int linkId);

    /**
     * @brief すべてクリア
     */
    void Clear();

    /**
     * @brief サンプルグラフの生成（デバッグ用）
     */
    void CreateSampleGraph();

    /**
     * @brief グリッド表示の切り替え
     */
    void SetShowGrid(bool show) { showGrid_ = show; }
    bool GetShowGrid() const { return showGrid_; }

    /**
     * @brief 自動整列の有効/無効
     */
    void SetAutoArrange(bool enable) { autoArrange_ = enable; }
    bool GetAutoArrange() const { return autoArrange_; }
};

} // namespace Tako

#endif // _DEBUG