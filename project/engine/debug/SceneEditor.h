#pragma once

#ifdef _DEBUG

#include "EditTracker.h"
#include "ShortcutTable.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Tako {

  class Actor;
  class BaseScene;
  class Camera;
  class Component;
  struct Matrix4x4;
  struct Vector3;

  // Hierarchy の行をドラッグしたときのデータ（ActorId）
  inline constexpr const char* kActorPayload = "TAKO_ACTOR";

  /// <summary>
  /// 今のシーンの Actor を Hierarchy と Inspector で編集する。シーンを変える操作は予約し、Update（描画の前）で行う。
  /// 複数選べる（Ctrl / Shift クリック）。Inspector とギズモの相手は、最後に選んだ 1 つ
  /// </summary>
  class SceneEditor {
  public: //メンバー関数
    SceneEditor() = default;
    // tracker_ が this を持つので複製しない
    SceneEditor(const SceneEditor&) = delete;
    SceneEditor& operator=(const SceneEditor&) = delete;

    /// <summary>
    /// Hierarchy に Actor の一覧と追加・削除のメニューを描く
    /// </summary>
    void DrawHierarchy();

    /// <summary>
    /// Inspector に選んだ Actor とコンポーネントを描く
    /// </summary>
    void DrawInspector();

    /// <summary>
    /// ギズモの種類（移動・回転・拡大）とローカル / ワールドの切り替えを 1 行に描く
    /// </summary>
    void DrawGizmoToolbar();

    /// <summary>
    /// 最後に選んだ Actor のギズモを、今のウィンドウの画像の上に描く。動かした結果は Actor の Transform（親からの相対）に戻す。
    /// 一緒に選んだ Actor も同じだけ動く
    /// </summary>
    /// <param name="camera">画像を描いたカメラ</param>
    /// <returns>ギズモの上にカーソルがあるか、掴んでいれば true（カメラ操作に入力を渡さないため）</returns>
    bool DrawGizmo(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageMax);

    /// <summary>
    /// カーソルの下で一番手前にあるメッシュの Actor を選ぶ。メッシュが無ければ原点の近くの Actor、それも無ければ選択を外す。Ctrl を押していれば、選択に足す・外す
    /// </summary>
    /// <param name="camera">画像を描いたカメラ</param>
    void PickActor(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageMax);

    /// <summary>
    /// 選んだ Actor のワールドでの位置
    /// </summary>
    /// <returns>選んでいなければ false</returns>
    bool GetSelectedPosition(Vector3& position) const;

    /// <summary>
    /// 予約した操作を行う。DebugUIManager::Update から呼ぶ（前のフレームの GPU 処理が終わった後なので消してよい）
    /// </summary>
    void Update();

    /// <summary>
    /// 編集して未保存なら out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    void ClearSelection()
    {
      selected_ = 0;
      selection_.clear();
    }

    /// <summary>
    /// 型かプレハブの名前の Actor を、Update で今のシーンに出す
    /// </summary>
    void RequestSpawn(const std::string& typeOrPrefab);

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<SceneEditor>> Shortcuts();

    //=====================================
    //Getter
    //=====================================
    /// <summary>
    /// 選んだ Actor が今のシーンにあれば true
    /// </summary>
    bool HasSelection() const;

  private: //非公開関数
    /// <summary>
    /// シーンを変える操作を Update まで遅らせる（ImGui を描く時点では、GPU がまだ今のフレームの物を使っている）
    /// </summary>
    void Request(std::function<void(BaseScene&)> operation);

    /// <summary>
    /// シーンファイルへ保存する（Edit 中だけ）
    /// </summary>
    bool Save();

    /// <summary>
    /// シーンが替わったら、未保存の印と履歴を捨てる
    /// </summary>
    void SyncTracker(const BaseScene& scene);

    void DrawCreateMenu();

    /// <summary>
    /// 名前を入れて、カメラと平行光源だけのシーンを作って開く
    /// </summary>
    void DrawNewScenePopup();

    /// <summary>
    /// Hierarchy の 1 行と、その子の行。別の行をここへ落とすと子になる
    /// </summary>
    void DrawActorRow(Actor& actor);

    /// <summary>
    /// child の親を parent にする（0 で外す）。Update で行う
    /// </summary>
    void RequestParent(uint64_t child, uint64_t parent);

    /// <summary>
    /// 行を parent へ落とした。落とした行が選択の中なら、選んだ全部の親を parent にする
    /// </summary>
    void RequestParentDropped(uint64_t dragged, uint64_t parent);

    /// <summary>
    /// 行やビューのクリックで選ぶ
    /// </summary>
    /// <param name="adds">Ctrl：今の選択に足す。もう選んであれば外す</param>
    /// <param name="extends">Shift：最後に選んだ物との間（Hierarchy の並び）を全部選ぶ</param>
    void Select(BaseScene& scene, uint64_t id, bool adds, bool extends);

    /// <summary>
    /// 選んだ全部を消す（Update で行う）
    /// </summary>
    void RequestDestroySelection();

    /// <summary>
    /// 選んだ全部を、子ごと複製する（Update で行う）
    /// </summary>
    void RequestDuplicateSelection();

    /// <summary>
    /// 最後に選んだ Actor のワールド行列が before から after に変わった分を、選んだ全部に掛ける（ギズモの結果）
    /// </summary>
    void MoveSelection(const Matrix4x4& before, const Matrix4x4& after);

    /// <summary>
    /// Play 中の値を、Stop の後も残す印を付ける・外す。component が空なら Actor 自身の値（名前・Active・Transform）
    /// </summary>
    void ToggleKeep(Actor& actor, const std::string& component);

    /// <summary>
    /// 印を付けた物の値を、Stop で作り直したシーンの同じ Actor・同じコンポーネントへ読み込む（まとめて Undo の 1 手）
    /// </summary>
    void ApplyKept(BaseScene& scene);

    /// <summary>
    /// 名前を付けた Actor に、コンポーネントを 1 つ付けて出す（+ Add の定番と、Assets からのドロップ）
    /// </summary>
    /// <param name="properties">コンポーネントの TPROPERTY に読ませる JSON の文字列</param>
    void RequestSpawnWith(const std::string& name, const std::string& componentType, const std::string& properties);

    /// <summary>
    /// 開いているドロップ先で、Assets からのプレハブとモデルを受け取って Actor を出す
    /// </summary>
    void AcceptSpawnDrops();

    /// <summary>
    /// actor と子孫を複製する（Update で行う）。組の中の親と ActorRef は、複製どうしを指す
    /// </summary>
    void RequestDuplicate(uint64_t actor);

    /// <summary>
    /// 選んだ全部と子孫を、クリップボードに JSON の文字列で置く
    /// </summary>
    void CopySelection();

    /// <summary>
    /// クリップボードの Actor を今のシーンに出す（別のシーンで写した物も貼れる）
    /// </summary>
    void PasteClipboard();

    void DrawActorMenu(Actor& actor);

    void DrawComponent(Actor& actor, Component& component);

    /// <summary>
    /// コンポーネントの見出しの右クリック。値をクリップボードに写す・同じ型の値を貼る（Play 中に写して、Stop の後に貼れる）
    /// </summary>
    void DrawComponentMenu(Actor& actor, Component& component);

    /// <summary>
    /// TCLASS(Requires = ...) の相手が同じ Actor に無ければ、警告と Add ボタンを出す。スクリプトは中身の型で見る
    /// </summary>
    void DrawRequirement(Actor& actor, Component& component);

    void DrawAddComponent(Actor& actor);

    void RequestAddComponent(uint64_t actor, const std::string& typeName, const std::string& name);

    std::string SerializeScene() const;

    /// <summary>
    /// Undo / Redo で text の内容に戻す（Update で行う）
    /// </summary>
    void RestoreScene(const std::string& text);

    //=====================================
    //Getter
    //=====================================
    static BaseScene* GetScene();

    /// <summary>
    /// 最後に選んだ Actor
    /// </summary>
    Actor* GetSelected() const;

    bool IsSelected(uint64_t id) const;

    /// <summary>
    /// 選んだ Actor のうち、先祖が選ばれていない物（Hierarchy の並び）。子ごと行う操作は、これに掛ける
    /// </summary>
    std::vector<Actor*> GetTopLevelSelection() const;

    bool IsKept(uint64_t actor, const std::string& component) const;

    uint64_t GetSceneRevision() const;

  private: //メンバー変数
    uint64_t                                     selected_       = 0;     ///< 最後に選んだ Actor の ID（作り直しをまたいでも同じ）
    std::vector<uint64_t>                        selection_;              ///< 選んだ全部（selected_ を含む）
    uint64_t                                     pressedRow_     = 0;     ///< 選んである行を素のクリックで押した。ドラッグせずに離したら、その行だけを選ぶ
    std::vector<std::function<void(BaseScene&)>> requests_;
    std::array<char, 64>                         prefabName_{};           ///< 「プレハブとして保存」の名前
    std::array<char, 64>                         newSceneName_{};
    std::string                                  trackedScene_;
    int                                          gizmoOperation_ = 0;     ///< 0 移動 / 1 回転 / 2 拡大
    bool                                         isGizmoLocal_   = true;

    //Play 中の値を Stop の後も残す
    std::map<std::pair<uint64_t, std::string>, std::string> kept_;                ///< （Actor の ID, コンポーネントの名前）→ 最後に控えた値（JSON の文字列）
    std::string                                             keptScene_;           ///< 印を付けたシーン（Stop で別のシーンへ戻ったら当てない）
    bool                                                    wasPlaying_ = false;

    //未保存と Undo（Edit 中だけ。Play 中の変更は Stop で捨てる。残す印を付けた値だけ、Stop の後に 1 手として入る）
    EditTracker<> tracker_{ { [this] { return SerializeScene(); },
                              [this](const std::string& text) { RestoreScene(text); },
                              [this] { return GetSceneRevision(); } } };
  };

} // namespace Tako

#endif // _DEBUG
