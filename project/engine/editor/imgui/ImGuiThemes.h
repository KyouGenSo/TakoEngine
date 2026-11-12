#pragma once

#ifdef _DEBUG

#include "imgui.h"

#include <span>

namespace Tako {

  /// <summary>
  /// ImGui のテーマ（配色）。形（余白・間隔・角丸）は全テーマ共通
  /// </summary>
  struct ImGuiTheme {
    using ApplyFunc = void (*)(ImGuiStyle& style);

    const char* name;   ///< 設定ウィンドウに表示する名前
    ApplyFunc   apply;  ///< 既定値の style に配色を設定する（形を書いても使われない）
  };

  /// <summary>
  /// テーマが持たない色。今のテーマの背景が暗いか明るいかで決まる
  /// </summary>
  struct EditorColors {
    ImVec4 success;
    ImVec4 warning;
    ImVec4 error;
    ImVec4 info;
    ImVec4 toggleOn;  ///< ON の切り替えボタンの背景
  };

  /// <summary>
  /// 選択可能なテーマ一覧。個人設定には name で保存するため name は一意にする
  /// </summary>
  TAKO_API std::span<const ImGuiTheme> GetImGuiThemes();

  /// <summary>
  /// 今のテーマに合う状態の色（ApplyImGuiTheme が決める）
  /// </summary>
  TAKO_API const EditorColors& GetEditorColors();

  /// <summary>
  /// ImGuiStyle を共通の形に戻してから index のテーマの配色を適用する
  /// </summary>
  TAKO_API void ApplyImGuiTheme(int index);

} // namespace Tako

#endif // _DEBUG
