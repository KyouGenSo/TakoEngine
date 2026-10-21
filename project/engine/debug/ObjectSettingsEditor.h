#pragma once

#ifdef _DEBUG

#include <array>
#include <span>
#include <string>
#include <vector>

#include "EditTracker.h"
#include "ObjectSettings.h"
#include "ShortcutTable.h"

namespace Tako {

  /// <summary>
  /// ObjectSettings（Object3d の名前ごとの描画設定）の編集ウィンドウ。生存中の Object3d を名前でまとめて並べる
  /// </summary>
  class ObjectSettingsEditor {
  public: //メンバー関数
    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="isOpen">ウィンドウ表示フラグ（DebugUIManager の表示状態を共有する）</param>
    void Initialize(bool* isOpen) { isOpen_ = isOpen; }

    void Draw();

    /// <summary>
    /// 編集して未保存なら out に追加する（終了時の確認用）
    /// </summary>
    void CollectUnsavedChanges(std::vector<std::string>& out) const;

    /// <summary>
    /// キー操作の表。Draw のキー処理と About の一覧が使う
    /// </summary>
    static std::span<const ShortcutBinding<ObjectSettingsEditor>> Shortcuts();

  private: //非公開関数
    bool Save();

    void DrawListPane();

    void DrawSettingsPane();

  private: //メンバー変数
    bool*                isOpen_ = nullptr;  ///< ウィンドウ表示フラグ（DebugUIManager 所有）
    std::string          selected_;          ///< 編集中の名前
    std::array<char, 64> newName_{};         ///< 一覧に足す名前（まだ生成されていない Object3d 用）

    //未保存と Undo
    EditTracker<> tracker_{ { [] { return ObjectSettings::GetInstance()->Serialize(); },
                              [](const std::string& text) { ObjectSettings::GetInstance()->Apply(text); },
                              [] { return static_cast<uint64_t>(ObjectSettings::GetInstance()->GetVersion()); } } };
  };

} // namespace Tako

#endif // _DEBUG
