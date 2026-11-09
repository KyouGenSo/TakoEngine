#pragma once

#ifdef _DEBUG

#include "EditTracker.h"
#include "ShortcutTable.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace Tako {

  class Actor;
  class BaseScene;
  class Component;

  /// <summary>
  /// 今のシーンの Actor を Hierarchy と Inspector で編集する。シーンを変える操作は予約し、Update（描画の前）で行う
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
    /// <returns>Actor を選んだフレームは true（他の選択を外すため）</returns>
    bool DrawHierarchy();

    /// <summary>
    /// Inspector に選んだ Actor とコンポーネントを描く
    /// </summary>
    void DrawInspector();

    /// <summary>
    /// 予約した操作を行う。DebugUIManager::Update から呼ぶ（前のフレームの GPU 処理が終わった後なので消してよい）
    /// </summary>
    void Update();

    /// <summary>
    /// 編集して未保存なら out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    void ClearSelection() { selected_ = 0; }

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

    void DrawActorMenu(Actor& actor);

    void DrawComponent(Actor& actor, Component& component);

    void DrawAddComponent(Actor& actor);

    std::string SerializeScene() const;

    /// <summary>
    /// Undo / Redo で text の内容に戻す（Update で行う）
    /// </summary>
    void RestoreScene(const std::string& text);

    //=====================================
    //Getter
    //=====================================
    static BaseScene* GetScene();

    Actor* GetSelected() const;

    uint64_t GetSceneRevision() const;

  private: //メンバー変数
    uint64_t                                     selected_ = 0;  ///< 選んだ Actor の ID（作り直しをまたいでも同じ）
    std::vector<std::function<void(BaseScene&)>> requests_;
    std::array<char, 64>                         prefabName_{};  ///< 「プレハブとして保存」の名前
    std::string                                  trackedScene_;

    //未保存と Undo（Edit 中だけ。Play 中の変更は Stop で捨てる）
    EditTracker<> tracker_{ { [this] { return SerializeScene(); },
                              [this](const std::string& text) { RestoreScene(text); },
                              [this] { return GetSceneRevision(); } } };
  };

} // namespace Tako

#endif // _DEBUG
