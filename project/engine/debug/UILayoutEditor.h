#pragma once

#ifdef _DEBUG

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "imgui.h"
#include "EditTracker.h"
#include "ShortcutTable.h"
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
    static constexpr float  kHierarchyPaneWidth   = 240.0f;
    static constexpr float  kHandleHalfSize       = 4.0f;    ///< 角と辺のハンドルの描画・当たり判定の半径（画像上 px）
    static constexpr float  kMinGridSpacing       = 4.0f;    ///< 画像上でこれより細かいグリッドは描かない（px）
    static constexpr float  kPreviewDefaultHeight = 240.0f;  ///< Inspector のテクスチャプレビューの初期の高さ（px）。下の縁のドラッグで変えられる
    static constexpr float  kResultDefaultHeight  = 160.0f;  ///< Inspector の 9-slice の結果プレビューの初期の高さ（px）
    static constexpr float  kGuideSnapDistance    = 6.0f;    ///< スマートガイドに吸着する距離（画像上 px）

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
      Paste,
      MoveUp,
      MoveDown,
      Reparent,
      Reload,
      Undo,
      Redo
    };

    /// <summary>
    /// Hierarchy で要素を落とした位置。行の上下 1/4 なら前後、それ以外なら子にする
    /// </summary>
    enum class DropPlace {
      Inside,
      Before,
      After
    };

    /// <summary>
    /// 保留中の操作。要素はフレームをまたいでも追えるよう名前で持つ
    /// </summary>
    struct Pending {
      PendingOp                op           = PendingOp::None;
      std::vector<std::string> targets;                 ///< 操作対象（Add 系と Paste では追加先の 1 つ）。空ならルート
      std::string              dropTarget;              ///< Reparent の基準の要素（空ならルート）
      DropPlace                dropPlace    = DropPlace::Inside;
      std::string              texture;                 ///< AddTexture / SetTexture のテクスチャキー
      Vector2                  clientPos    = {};       ///< AddTexture の配置中心（クライアント px）
      bool                     hasClientPos = false;    ///< false なら追加先の親矩形の中心に置く
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

    /// <summary>
    /// 編集を見つける区間の始まりの状態
    /// </summary>
    struct EditMark {
      uint64_t                 layoutHash;  ///< UILayout::Hash()
      uint64_t                 slicesHash;  ///< UILayout::HashTextureSlices()
      std::vector<std::string> selection;   ///< Undo で戻す選択
    };

    /// <summary>
    /// Undo / Redo で戻す状態
    /// </summary>
    struct EditState {
      std::string              layout;     ///< UILayout::Serialize()
      std::string              slices;     ///< UILayout::SerializeTextureSlices()
      std::vector<std::string> selection;

      /// <summary>
      /// 選択だけの違いは 1 手にしない
      /// </summary>
      bool operator==(const EditState& other) const { return layout == other.layout && slices == other.slices; }
    };

    /// <summary>
    /// 選択中の要素をそろえる位置。Distribute は間の隙間を等しくする
    /// </summary>
    enum class AlignOp {
      Left,
      CenterX,
      Right,
      Top,
      CenterY,
      Bottom,
      DistributeX,
      DistributeY
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
    /// エディタウィンドウを描画し、矢印キー移動とショートカットを処理する
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

    /// <summary>
    /// キー操作とマウス操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<UILayoutEditor>> Shortcuts();

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
    /// 編集対象を切り替える（新規作成は次の Update で行う）。未保存の印と Undo の履歴は捨てる
    /// </summary>
    void Open(const OpenRequest& request);

    /// <summary>
    /// 保存して結果をログに出す。テクスチャごとの共有の 9-slice に変更があれば一緒に保存する。成功したら未保存の印を外す
    /// </summary>
    /// <returns>書き込めたら true</returns>
    bool SaveLayout(const UILayout& layout);

    /// <summary>
    /// Inspector で編集する要素（選択の末尾）
    /// </summary>
    UIElement* FindSelected(UILayout& layout) const;

    bool IsSelected(const std::string& name) const;

    /// <summary>
    /// 選択を element 1 つにする（null なら選択を外す）
    /// </summary>
    void Select(const UIElement* element);

    /// <summary>
    /// element を選択に足す（選択中なら外す）。足した要素が Inspector の対象になる
    /// </summary>
    void ToggleSelect(const UIElement& element);

    /// <summary>
    /// 選択を置き換え、Inspector の要素ごとの状態（名前の入力・プレビューの表示範囲）を戻す
    /// </summary>
    void SetSelection(std::vector<std::string> names);

    /// <summary>
    /// 選択中の要素を前順に集める。選択中の要素の子孫は、親と一緒に動くので除く
    /// </summary>
    std::vector<UIElement*> SelectedTopLevel(UILayout& layout) const;

    void DrawToolbar(UILayout* layout);

    /// <summary>
    /// 選択中の要素をそろえるボタンの行
    /// </summary>
    void DrawAlignButtons(UILayout& layout);

    /// <summary>
    /// 1 つなら親の矩形に、2 つ以上なら選択を囲む矩形にそろえる。画面上の位置だけを変える（回転は見ない）
    /// </summary>
    void AlignSelection(UILayout& layout, AlignOp op);

    /// <summary>
    /// 名前を検証して pendingNewLayout_ に積む。実際の作成は次の Update
    /// </summary>
    void DrawNewLayoutPopup();

    void DrawHierarchy(UILayout& layout);
    void DrawTreeNode(UILayout& layout, UIElement& element);

    /// <summary>
    /// Hierarchy の行のクリック。Ctrl で選択に足す・外す、Shift でアンカーからの範囲を選ぶ（範囲は全行を描いた後に決める）
    /// </summary>
    void HandleHierarchyClick(const UIElement& element);

    /// <summary>
    /// 直前のアイテムを、要素（並べ替え・親の付け替え）とテクスチャ（新規追加）のドロップ先にする
    /// </summary>
    /// <param name="element">行の要素（null なら空き領域。ルートの末尾へ入れる）</param>
    void AcceptHierarchyDrop(UILayout& layout, const UIElement* element);
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
    /// UI の操作の前に呼び、TrackLayoutEdit に渡す。ゲームのコードが前の区間の後に内容を変えていたら、ここで取り直す
    /// </summary>
    EditMark MarkEdit(const UILayout& layout);

    /// <summary>
    /// UI の操作の前後で内容が変わっていれば、未保存の印を付ける。操作で未保存でなかった時点の内容に戻ったら外す。
    /// 変わったかどうかはハッシュで調べ、変わったときだけ Serialize する
    /// </summary>
    /// <param name="mark">操作の前の MarkEdit(layout)</param>
    /// <param name="isUndoable">false なら Undo の履歴に積まない（Undo / Redo 自身の操作）</param>
    void TrackLayoutEdit(const UILayout& layout, const EditMark& mark, bool isUndoable = true);

    EditState CaptureState(const UILayout& layout) const;

    /// <summary>
    /// レイアウト・共有の 9-slice・選択を state に戻す。スプライトを作り直すので Update（ApplyPending）で呼ぶ
    /// </summary>
    void RestoreState(UILayout& layout, const EditState& state);

    /// <summary>
    /// 選択中の要素があれば、選択全体に操作を登録する（RequestOnSelection）
    /// </summary>
    void RequestOnSelected(PendingOp op);

    /// <summary>
    /// 選択中の要素を子孫ごと clipboard_ に写す
    /// </summary>
    void CopySelection(UILayout& layout);

    /// <summary>
    /// 次フレームの Update で実行する操作を登録する（1 フレームに 1 つ。後から登録したものが優先）
    /// </summary>
    /// <param name="target">操作対象の要素名（空ならルート）</param>
    void Request(PendingOp op, const std::string& target, const std::string& texture = {});

    /// <summary>
    /// name が選択中なら選択全体（SelectedTopLevel）に、そうでなければ name だけに操作を登録する
    /// </summary>
    void RequestOnSelection(PendingOp op, UILayout& layout, const std::string& name);

    /// <summary>
    /// テクスチャを原寸で parent の子として追加し、clientCenter を中心に置いて選択する
    /// </summary>
    void CreateFromTexture(UILayout& layout, UIElement& parent, const std::string& texture, const Vector2& clientCenter);

    /// <summary>
    /// 描画順で最前面にある、表示中かつテクスチャを持ちロックされていない要素のうち clientPos を含むもの
    /// </summary>
    UIElement* PickTopmost(UILayout& layout, const Vector2& clientPos) const;

    /// <summary>
    /// ビューポートのドラッグで、選択中の要素をまとめて移動するか、Inspector の要素をリサイズする
    /// </summary>
    /// <param name="guideDistance">スマートガイドに吸着する距離（クライアント px）</param>
    void UpdateDrag(UILayout& layout, const Vector2& mouse, float guideDistance);

    /// <summary>
    /// スマートガイドの線（クライアント px）。画面と、Inspector の要素の親・選択していない表示中の兄弟の、端と中央
    /// </summary>
    void CollectGuideLines(UILayout& layout, std::vector<float>& xs, std::vector<float>& ys) const;

    void HandleArrowKeys(const std::vector<UIElement*>& elements) const;

  private: //メンバー変数
    bool* isOpen_ = nullptr;

    //選択（Reload やシーン再生成で要素が作り直されても追えるよう名前で持つ）
    std::string              layoutName_;
    std::vector<std::string> selection_;          ///< 末尾が Inspector で編集する要素
    std::string              rangeAnchor_;        ///< Hierarchy の Shift+クリックの範囲の起点（最後に Shift なしでクリックした要素）
    std::string              rangeSelectTarget_;  ///< 今フレームの Shift+クリックで範囲の終点にする要素
    std::vector<std::string> hierarchyRows_;      ///< 今フレームの Hierarchy に描いた行（上から順）

    //プレビュー（今のシーンが読み込んでいないレイアウトを、エディタ自前のインスタンスで開く）
    std::unique_ptr<UILayout> preview_;
    bool                      isPreviewMode_       = false;  ///< 編集対象をプレビューで開くか
    std::string               pendingNewLayout_;             ///< 次の Update で作る新規レイアウト名
    char                      newLayoutBuffer_[64] = "";
    std::string               newLayoutError_;               ///< 新規作成ポップアップに出す検証エラー

    //未保存の変更
    const UILayout*      editedLayout_       = nullptr;  ///< 未保存の変更があるインスタンス。シーンが作り直した同名のレイアウトと区別するためポインタで持つ
    std::string          cleanSnapshot_;                 ///< editedLayout_ が付く直前の内容。編集でここへ戻れば未保存を外す
    std::string          cleanSliceSnapshot_;            ///< isSliceTableEdited_ が付く直前の共有の 9-slice の内容
    UnsavedChangesPrompt unsavedPrompt_;                 ///< 未保存のまま別のレイアウトへ切り替えるときの確認
    std::string          discardLayout_;                 ///< 次の Update でファイルから読み直して編集を捨てるゲーム側のレイアウト

    //編集の検出（Serialize は重いので、ハッシュが変わったときだけ作り直す）
    uint64_t    cachedLayoutHash_ = 0;
    std::string cachedLayout_;          ///< 最後に Serialize した編集対象の内容。区間の間は操作の前の内容
    uint64_t    cachedSlicesHash_ = 0;
    std::string cachedSlices_;          ///< 最後に SerializeTextureSlices した内容

    //Undo / Redo（開いているレイアウト 1 つの編集だけを持つ）
    EditHistory<EditState> history_;
    const UILayout*        historyLayout_ = nullptr;  ///< 履歴の対象。別のインスタンスを編集したら履歴を捨てる
    std::string            clipboard_;                ///< コピーした要素（UILayout::SerializeElements）

    //名前の編集
    char renameBuffer_[128]      = "";
    bool isRenaming_             = false;
    bool isRenameRejected_       = false;  ///< 直前の改名が空または重複で取り消されたか
    bool isRenameFocusRequested_ = false;  ///< 次に Inspector を描くとき Name 欄に入力を移す

    //Hierarchy
    char hierarchyFilter_[64] = "";  ///< 空でなければ、名前に含む要素だけを平らに並べる

    //ビューポート操作
    bool                                         isViewportEditEnabled_ = true;
    DragMode                                     dragMode_              = DragMode::None;
    int                                          dragEdges_             = 0;               ///< リサイズ中の辺（HitEdges の bit の和）
    Vector2                                      dragStartMouse_        = {};              ///< ドラッグ開始時のマウス（クライアント px）
    Vector2                                      dragStartRectMin_      = {};              ///< ドラッグ開始時の、動かす矩形（移動は選択を囲む矩形、リサイズは要素の矩形。クライアント px）
    Vector2                                      dragStartRectSize_     = {};
    std::vector<std::pair<std::string, Vector2>> dragStartMins_;                           ///< 移動する要素ごとの、ドラッグ開始時の画面矩形の左上
    bool                                         hasDragged_            = false;           ///< 今回の押下でドラッグしたか
    bool                                         isClickToPick_         = false;           ///< ドラッグせずに離したら、クリック位置の要素を選び直すか
    bool                                         isViewportFocused_     = false;           ///< 今フレーム Game Viewport がフォーカス中か（DrawViewportOverlay で更新、Draw で消費）

    //グリッド・スナップ・スマートガイド
    bool                 isGridVisible_   = false;
    bool                 isSnapEnabled_   = false;
    float                gridSize_        = 10.0f;  ///< 基準解像度 px
    bool                 isGuidesEnabled_ = true;   ///< グリッドのスナップを使わない間、ドラッグ中に端と中央へ吸着する
    std::optional<float> guideX_;                   ///< 今フレーム吸着した縦の線（クライアント px）
    std::optional<float> guideY_;

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
