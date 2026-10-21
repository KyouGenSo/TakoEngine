#pragma once

#ifdef _DEBUG

#include <array>
#include <span>
#include <string>
#include <vector>

#include "EditTracker.h"
#include "MaterialLibrary.h"
#include "ShortcutTable.h"

namespace Tako {

  class Material;

  /// <summary>
  /// .mat.json の編集ウィンドウ。値の UI はシェーダーのリフレクションから作る（Object3d への割り当ては ObjectSettingsEditor）
  /// </summary>
  class MaterialEditor {
  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    void Draw();

    /// <summary>
    /// 編集して未保存なら、そのマテリアルを out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<MaterialEditor>> Shortcuts();

  private: //非公開関数
    /// <summary>
    /// name を読み込んで編集対象にする（Undo の履歴は捨てる）
    /// </summary>
    void Select(const std::string& name);

    bool Save();

    void DrawListPane();

    void DrawMaterialPane(Material& material);

  private: //メンバー変数
    bool*                isOpen_     = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    std::string          selected_;              ///< 編集中のマテリアル名
    std::array<char, 64> newName_{};             ///< 新規作成するマテリアル名
    std::string          newShader_  = "Lit";    ///< 新規作成に使うシェーダー

    //未保存と Undo
    EditTracker<>        tracker_{ { [this] { return MaterialLibrary::GetInstance()->Serialize(selected_); },
                                     [this](const std::string& text) { MaterialLibrary::GetInstance()->Apply(selected_, text); } } };
    UnsavedChangesPrompt unsavedPrompt_;  ///< 未保存のまま別のマテリアルを開くときの確認
  };

} // namespace Tako

#endif // _DEBUG
