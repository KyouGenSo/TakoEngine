#pragma once

#ifdef _DEBUG

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "imgui.h"
#include "Vector2.h"
#include "Vector4.h"

namespace Tako {

  class UILayout;
  struct UIElement;
  struct UISlice;

  /// <summary>
  /// 生存中の UILayout を編集するエディタ（Hierarchy / Inspector のウィンドウと、Game Viewport 上のギズモ）
  /// </summary>
  class UILayoutEditor {
  private: //定数
    static constexpr float kHierarchyPaneWidth   = 240.0f;
    static constexpr float kHandleHalfSize       = 4.0f;    ///< 角ハンドルの描画・当たり判定の半径（画像上 px）
    static constexpr float kMinGridSpacing       = 4.0f;    ///< 画像上でこれより細かいグリッドは描かない（px）
    static constexpr float kPreviewDefaultHeight = 240.0f;  ///< Inspector のテクスチャプレビューの初期の高さ（px）。下の縁のドラッグで変えられる
    static constexpr float kResultDefaultHeight  = 160.0f;  ///< Inspector の 9-slice の結果プレビューの初期の高さ（px）

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

    /// <summary>
    /// 編集対象の切り替え先
    /// </summary>
    struct OpenRequest {
      std::string name;
      bool        isPreview  = false;  ///< シーン外のファイルをプレビューで開くか
      bool        isNew      = false;  ///< name で新規作成するか
      std::string selectName;          ///< 開いた後に選ぶ要素名（空なら選ばない）
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

    /// <summary>
    /// 未保存の変更があるレイアウトを out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

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

    /// <summary>
    /// 未保存の変更があれば確認モーダルを予約し、無ければすぐ開く
    /// </summary>
    void RequestOpen(const OpenRequest& request);

    /// <summary>
    /// 編集対象を切り替える（新規作成は次の Update で行う）。未保存の印は外す
    /// </summary>
    void Open(const OpenRequest& request);

    /// <summary>
    /// 保存して結果をログに出す。テクスチャごとの共有の 9-slice に変更があれば一緒に保存する。成功したら未保存の印を外す
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool SaveLayout(const UILayout& layout);

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
    /// 9-slice の枠・区画の設定と、要素のサイズに対する警告を描く
    /// </summary>
    /// <param name="slice">編集する値（呼び出し側が共有か要素の上書きへ書き戻す）</param>
    void DrawSliceSettings(const UILayout& layout, UIElement& element, UISlice& slice);

    /// <summary>
    /// element と同じテクスチャを使う要素（開いている全レイアウト）を一覧にするボタンとポップアップ。選ぶとその要素へ移動する
    /// </summary>
    void DrawSliceUsers(const UIElement& element);

    /// <summary>
    /// テクスチャと切り出し範囲（黄）・9-slice の線（緑）を、拡大・移動できるキャンバスに描く。
    /// 左ドラッグで 9-slice の線または切り出し範囲を動かし、9-slice モードのクリックで区画を選ぶ
    /// </summary>
    /// <param name="slice">編集する 9-slice（呼び出し側が書き戻す）</param>
    void DrawTexturePreview(UIElement& element, UISlice& slice);

    /// <summary>
    /// 要素の今の画面サイズで 9-slice を切った結果を、ゲームと同じ計算（Sprite::CalcSliceGrid）で描く。反転と回転は反映しない
    /// </summary>
    void DrawSliceResult(const UILayout& layout, const UIElement& element, const UISlice& slice);

    void ApplyPending(UILayout& layout);

    /// <summary>
    /// UI の操作の前後で内容が変わっていれば、未保存の印を付ける。操作で未保存でなかった時点の内容に戻ったら外す
    /// </summary>
    /// <param name="before">操作の前の layout.Serialize()</param>
    void TrackLayoutEdit(const UILayout& layout, const std::string& before);

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

    //未保存の変更
    const UILayout*            editedLayout_            = nullptr;  ///< 未保存の変更があるインスタンス。シーンが作り直した同名のレイアウトと区別するためポインタで持つ
    std::string                cleanSnapshot_;                      ///< editedLayout_ が付く直前の内容。編集でここへ戻れば未保存を外す
    std::string                cleanSliceSnapshot_;                 ///< isSliceTableEdited_ が付く直前の共有の 9-slice の内容
    std::optional<OpenRequest> pendingOpen_;                        ///< 確認モーダルの返答待ちの切り替え先
    bool                       isUnsavedPopupRequested_ = false;
    std::string                discardLayout_;                      ///< 次の Update でファイルから読み直して編集を捨てるゲーム側のレイアウト

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

    //テクスチャプレビュー
    float   previewZoom_           = 1.0f;  ///< プレビュー px ÷ テクスチャ px
    Vector2 previewOrigin_         = {};    ///< テクスチャ左上のキャンバス内の位置（プレビュー px）
    bool    isPreviewFit_          = true;  ///< true の間は毎フレーム、テクスチャと切り出し範囲が全部収まるように合わせる。拡大や移動をすると false
    bool    isSliceDragMode_       = true;  ///< 左ドラッグで動かすのが 9-slice の線か（false なら切り出し範囲）
    int     previewDragEdges_      = 0;     ///< ドラッグ中の辺（左 1・上 2・右 4・下 8 の和。0 ならドラッグしていない）
    Vector2 previewDragStartMouse_ = {};    ///< ドラッグ開始時のマウス（テクスチャ px）
    Vector4 previewDragStartLines_ = {};    ///< ドラッグ開始時の、動かす線の位置（テクスチャ px、左上右下）

    //9-slice
    int  selectedRegion_     = 4;      ///< 編集中の区画（UISlice の並び。4 は中央）
    bool isSliceMirrored_    = false;  ///< 左右・上下の枠を同じ値にそろえるか
    bool isSliceTableEdited_ = false;  ///< テクスチャごとの共有の 9-slice に未保存の変更があるか

    //キーボード
    bool wantsKeyboard_ = false;

    Pending pending_;
  };

} // namespace Tako

#endif // _DEBUG
