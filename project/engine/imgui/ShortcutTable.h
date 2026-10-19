#pragma once

#ifdef _DEBUG

#include "imgui.h"
#include "imgui_internal.h"

#include <span>

namespace Tako {

  /// <summary>
  /// ショートカットを受ける範囲
  /// </summary>
  enum class ShortcutScope {
    FocusedWindow,  ///< 呼んだウィンドウ（子ウィンドウを含む）にフォーカスがあるときだけ。入力欄の Ctrl+Z などが優先される
    Global          ///< フォーカスと文字の入力中かは呼び出し側で判定する
  };

  /// <summary>
  /// ショートカット 1 件。ProcessShortcuts のキー処理と DrawShortcutTable の一覧が同じ表を使う
  /// </summary>
  /// <typeparam name="Owner">action に渡す対象</typeparam>
  template <class Owner>
  struct ShortcutBinding {
    ImGuiKeyChord   chord       = ImGuiKey_None;
    ImGuiInputFlags flags       = ImGuiInputFlags_None;  ///< ImGuiInputFlags_Repeat なら押し続けで繰り返す
    const char*     description = "";
    void            (*action)(Owner& owner) = nullptr;   ///< nullptr なら一覧に出すだけ（処理は別の場所かライブラリ側）
    const char*     keyLabel    = nullptr;               ///< chord で書けない操作（"RMB drag" など）の表記
  };

  /// <summary>
  /// 表のキーが押されていれば action を呼ぶ。FocusedWindow ならウィンドウの Begin と End の間で呼ぶ
  /// </summary>
  template <class Owner>
  void ProcessShortcuts(std::span<const ShortcutBinding<Owner>> table, Owner& owner, ShortcutScope scope = ShortcutScope::FocusedWindow) {
    for (const ShortcutBinding<Owner>& binding : table) {
      if (!binding.action) {
        continue;
      }
      bool isPressed = false;
      if (scope == ShortcutScope::Global) {
        // ImGui::Shortcut は RouteAlways でもキーの持ち主を確かめるので、持ち主を見ない方で受ける
        isPressed = ImGui::IsKeyChordPressed(binding.chord, binding.flags);
      }
      // 修飾キーの無いキーは、文字の入力中なら入力欄に渡す（入力欄は Space を奪わない）
      else if ((binding.chord & ImGuiMod_Mask_) != 0 || !ImGui::GetIO().WantTextInput) {
        isPressed = ImGui::Shortcut(binding.chord, binding.flags);
      }
      if (isPressed) {
        binding.action(owner);
      }
    }
  }

  /// <summary>
  /// キーと説明の 2 列の表
  /// </summary>
  template <class Owner>
  void DrawShortcutTable(std::span<const ShortcutBinding<Owner>> table) {
    if (!ImGui::BeginTable("##Shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
      return;
    }
    for (const ShortcutBinding<Owner>& binding : table) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(binding.keyLabel ? binding.keyLabel : ImGui::GetKeyChordName(binding.chord));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(binding.description);
    }
    ImGui::EndTable();
  }

} // namespace Tako

#endif // _DEBUG
