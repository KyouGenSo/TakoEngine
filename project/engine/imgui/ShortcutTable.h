#pragma once

#ifdef _DEBUG

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

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
  /// メインメニュー（File > Save と Edit）から操作を送る相手。最後にフォーカスのあったエディタのウィンドウ
  /// </summary>
  struct MenuTarget {
    ImGuiID                    window  = 0;
    std::string                name;                     ///< メニューに出すウィンドウ名
    std::vector<ImGuiKeyChord> chords;                   ///< 相手の表にある操作（action のある物）
    int                        frame   = -1;             ///< chords を集めたフレーム。同じフレームの 2 つ目の表は足す
    int                        alive   = -1;             ///< 相手が最後にキーの処理をしたフレーム
    ImGuiKeyChord              pending = ImGuiKey_None;  ///< メニューで選んだ操作。相手がキーの処理の所で実行する

    /// <summary>
    /// 相手が chord の操作を今実行できるか。相手がキーの処理を止めている間（閉じた・Play 中など）は、表にあっても false
    /// </summary>
    bool Has(ImGuiKeyChord chord) const {
      // メニューは各ウィンドウより先に描くので、相手の最後の処理は 1 つ前のフレーム
      return ImGui::GetFrameCount() - alive <= 1 && std::ranges::find(chords, chord) != chords.end();
    }
  };

  inline MenuTarget& GetMenuTarget() {
    static MenuTarget target;
    return target;
  }

  /// <summary>
  /// フォーカスがあれば、メインメニューの相手として名乗る。自分が相手で、メニューで選ばれた操作が表にあれば取り出す
  /// </summary>
  /// <param name="window">相手を見分ける ID（ウィンドウの ID）</param>
  /// <returns>メニューで選ばれた操作（無ければ ImGuiKey_None）</returns>
  template <class Owner>
  ImGuiKeyChord ClaimMenuTarget(ImGuiID window, const char* name, bool isFocused, std::span<const ShortcutBinding<Owner>> table) {
    MenuTarget& target = GetMenuTarget();
    if (isFocused) {
      const int frame = ImGui::GetFrameCount();
      if (target.window != window || target.frame != frame) {
        // 相手が替わったら、前の相手へ送った操作は捨てる
        if (target.window != window) {
          target.pending = ImGuiKey_None;
        }
        target.window = window;
        target.frame  = frame;
        target.name.assign(name, ImGui::FindRenderedTextEnd(name));
        target.chords.clear();
      }
      for (const ShortcutBinding<Owner>& binding : table) {
        if (binding.action) {
          target.chords.push_back(binding.chord);
        }
      }
    }
    if (target.window != window) {
      return ImGuiKey_None;
    }
    target.alive = ImGui::GetFrameCount();
    if (target.pending == ImGuiKey_None) {
      return ImGuiKey_None;
    }
    // 自分の表に無い操作は、同じウィンドウの別の表のために残す
    const bool isMine = std::ranges::any_of(table, [&target](const ShortcutBinding<Owner>& binding) { return binding.action && binding.chord == target.pending; });
    return isMine ? std::exchange(target.pending, ImGuiKey_None) : ImGuiKey_None;
  }

  /// <summary>
  /// 表の chord の action を呼ぶ（メニューから送られた操作）。ImGuiKey_None なら何もしない
  /// </summary>
  template <class Owner>
  void RunShortcut(std::span<const ShortcutBinding<Owner>> table, Owner& owner, ImGuiKeyChord chord) {
    if (chord == ImGuiKey_None) {
      return;
    }
    for (const ShortcutBinding<Owner>& binding : table) {
      if (binding.action && binding.chord == chord) {
        binding.action(owner);
        return;
      }
    }
  }

  /// <summary>
  /// 表のキーが押されていれば action を呼ぶ。FocusedWindow ならウィンドウの Begin と End の間で呼ぶ（メインメニューから送られた操作もここで実行する）
  /// </summary>
  template <class Owner>
  void ProcessShortcuts(std::span<const ShortcutBinding<Owner>> table, Owner& owner, ShortcutScope scope = ShortcutScope::FocusedWindow) {
    if (scope == ShortcutScope::FocusedWindow) {
      const ImGuiWindow* window = ImGui::GetCurrentWindowRead()->RootWindow;
      RunShortcut(table, owner, ClaimMenuTarget(window->ID, window->Name, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows), table));
    }
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
