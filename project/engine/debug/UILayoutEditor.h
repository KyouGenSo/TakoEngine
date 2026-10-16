#pragma once

#ifdef _DEBUG

#include <memory>
#include <string>
#include "imgui.h"
#include "Vector2.h"

namespace Tako {

  class UILayout;
  struct UIElement;

  /// <summary>
  /// 生存中の UILayout を編集するエディタ（Hierarchy / Inspector のウィンドウと、Game Viewport 上のギズモ）
  /// </summary>
  class UILayoutEditor {
  private: //定数
    static constexpr float kHierarchyPaneWidth = 240.0f;
    static constexpr float kHandleHalfSize     = 4.0f;    ///< 角ハンドルの描画・当たり判定の半径（画像上 px）
    static constexpr float kMinGridSpacing     = 4.0f;    ///< 画像上でこれより細かいグリッドは描かない（px）
    static constexpr float kPreviewMaxHeight   = 160.0f;  ///< Inspector のテクスチャプレビューの最大高さ（px）

  private: //構造体
    /// <summary>
    /// ビューポート上のドラッグ操作
    /// </summary>
    enum class DragMode {
      None,
      Move,
      Resize
    };

    /// <summary>
    /// 要素の追加・削除・並べ替え・再読込。ImGui の描画はシーン描画の後なので、ここでスプライトを破棄すると
    /// 記録済みコマンドリストが参照する GPU バッファを解放してしまう。次フレームの Update（GPU 完了後）でまとめて行う
    /// </summary>
    enum class PendingOp {
      None,
      AddImage,
      AddGroup,
      AddTexture,
      SetTexture,
      Duplicate,
      Remove,
      MoveUp,
      MoveDown,
      Reparent,
      Reload
    };

    /// <summary>
    /// 保留中の操作。要素はフレームをまたいでも追えるよう名前で持つ（空ならルート）
    /// </summary>
    struct Pending {
      PendingOp   op           = PendingOp::None;
      std::string target;                  ///< 操作対象（Add 系では追加先の親）
      std::string newParent;               ///< Reparent の移動先
      std::string texture;                 ///< AddTexture / SetTexture のテクスチャキー
      Vector2     clientPos    = {};       ///< AddTexture の配置中心（クライアント px）
      bool        hasClientPos = false;    ///< false なら追加先の親矩形の中心に置く
    };

  public: //メンバー関数
    UILayoutEditor();
    ~UILayoutEditor();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    /// <summary>
    /// 新規作成・プレビューの開閉・保留中の操作を実行し、選択中レイアウトを Update する（F7 停止中でも編集結果を反映するため）。
    /// スプライトを破棄し得るので、GPU 完了後の更新フェーズで呼ぶこと
    /// </summary>
    void Update();

    /// <summary>
    /// エディタウィンドウを描画し、矢印キー移動を処理する
    /// </summary>
    void Draw();

    /// <summary>
    /// Game Viewport の画像の上にグリッドとギズモを描き、選択・移動・リサイズ・テクスチャのドロップを処理する。
    /// DebugUIManager::DrawGameViewport の ImGui::Image 直後に呼ぶ
    /// </summary>
    /// <param name="imageMin">ゲーム画像の左上（ImGui スクリーン座標）</param>
    /// <param name="imageMax">ゲーム画像の右下（ImGui スクリーン座標）</param>
    /// <returns>ギズモがマウスを使っていてゲームへクリックを渡すべきでない場合 true</returns>
    bool DrawViewportOverlay(const ImVec2& imageMin, const ImVec2& imageMax);

    /// <summary>
    /// シーン外のレイアウトをプレビュー編集中なら、ゲーム画面の上に重ねて描く。
    /// TakoFramework::Draw() の SceneManager::DrawWithoutEffect() 直後に呼ぶ
    /// </summary>
    void DrawPreview();

    //========================
    //Getter
    //========================
    /// <summary>
    /// 矢印キーで要素を動かせる状態か（ゲームへのキーボード入力を止めるのに使う）
    /// </summary>
    bool WantsKeyboard() const { return wantsKeyboard_; }

  private: //非公開関数
    /// <summary>
    /// 編集対象。layoutName_ と同名のゲーム側インスタンスを優先し、無ければプレビュー（無ければ nullptr）
    /// </summary>
    UILayout* FindLayout() const;

    /// <summary>
    /// ゲームのコードが読み込んだ同名のインスタンス（エディタのプレビューは除く）
    /// </summary>
    UILayout* FindGameLayout(const std::string& name) const;

    /// <summary>
    /// 新規作成の実行と、編集対象に合わせたプレビューの生成・破棄
    /// </summary>
    void SyncPreview();

    UIElement* FindSelected(UILayout& layout) const;
    void Select(const UIElement* element);

    void DrawToolbar(UILayout* layout);

    /// <summary>
    /// 名前を検証して pendingNewLayout_ に積む。実際の作成は次の Update
    /// </summary>
    void DrawNewLayoutPopup();

    void DrawHierarchy(UILayout& layout);
    void DrawTreeNode(UIElement& element);

    /// <summary>
    /// 直前のアイテムを、要素（親の付け替え）とテクスチャ（新規追加）のドロップ先にする
    /// </summary>
    /// <param name="parentName">ドロップ後の親の要素名（空ならルート）</param>
    void AcceptHierarchyDrop(const std::string& parentName);
    void DrawInspector(UILayout& layout, UIElement& element);

    /// <summary>
    /// Unity と同じ 4x4 のアンカープリセット。押しても画面上の矩形は保つ（Shift でピボットも、Alt で位置も合わせる）
    /// </summary>
    void DrawAnchorPresets(UILayout& layout, UIElement& element);

    /// <summary>
    /// テクスチャ全体を表示し、切り出し範囲（黄）と 9-slice の線（緑）を重ねる
    /// </summary>
    void DrawTexturePreview(const UIElement& element);

    void ApplyPending(UILayout& layout);

    /// <summary>
    /// 次フレームの Update で実行する操作を登録する（1 フレームに 1 つ。後から登録したものが優先）
    /// </summary>
    void Request(PendingOp op, const std::string& target, const std::string& newParent = {}, const std::string& texture = {});

    /// <summary>
    /// テクスチャを原寸で parent の子として追加し、clientCenter を中心に置いて選択する
    /// </summary>
    void CreateFromTexture(UILayout& layout, UIElement& parent, const std::string& texture, const Vector2& clientCenter);

    /// <summary>
    /// 描画順で最前面にある、表示中かつテクスチャを持ちロックされていない要素のうち clientPos を含むもの
    /// </summary>
    UIElement* PickTopmost(UILayout& layout, const Vector2& clientPos) const;

    /// <summary>
    /// ビューポートのドラッグで選択要素を移動・リサイズする
    /// </summary>
    void UpdateDrag(UILayout& layout, UIElement& element, const Vector2& mouse);

    void HandleArrowKeys(UIElement& element) const;

  private: //メンバー変数
    bool* isOpen_ = nullptr;

    //選択（Reload やシーン再生成で要素が作り直されても追えるよう名前で持つ）
    std::string layoutName_;
    std::string selectedName_;

    //プレビュー（今のシーンが読み込んでいないレイアウトを、エディタ自前のインスタンスで開く）
    std::unique_ptr<UILayout> preview_;
    bool                      isPreviewMode_       = false;  ///< 編集対象をプレビューで開くか
    std::string               pendingNewLayout_;             ///< 次の Update で作る新規レイアウト名
    char                      newLayoutBuffer_[64] = "";
    std::string               newLayoutError_;               ///< 新規作成ポップアップに出す検証エラー

    //名前の編集
    char renameBuffer_[128] = "";
    bool isRenaming_        = false;
    bool isRenameRejected_  = false;  ///< 直前の改名が空または重複で取り消されたか

    //ビューポート操作
    bool     isViewportEditEnabled_ = true;
    DragMode dragMode_              = DragMode::None;
    int      dragCorner_            = 0;               ///< リサイズ中の角（0:左上 1:右上 2:右下 3:左下）
    Vector2  dragStartMouse_        = {};              ///< ドラッグ開始時のマウス（クライアント px）
    Vector2  dragStartRectMin_      = {};              ///< ドラッグ開始時の画面矩形（クライアント px）
    Vector2  dragStartRectSize_     = {};
    bool     hasDragged_            = false;           ///< 今回の押下でドラッグしたか
    bool     isClickToPick_         = false;           ///< ドラッグせずに離したら、クリック位置の要素を選び直すか
    bool     isViewportFocused_     = false;           ///< 今フレーム Game Viewport がフォーカス中か（DrawViewportOverlay で更新、Draw で消費）

    //グリッド・スナップ
    bool  isGridVisible_ = false;
    bool  isSnapEnabled_ = false;
    float gridSize_      = 10.0f;  ///< 基準解像度 px

    //キーボード
    bool wantsKeyboard_ = false;

    Pending pending_;
  };

} // namespace Tako

#endif // _DEBUG
