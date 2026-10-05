#pragma once

#ifdef _DEBUG

#include <span>

struct ImGuiStyle;

namespace Tako {

  /// <summary>
  /// ImGui のテーマ（サイズと配色）
  /// </summary>
  struct ImGuiTheme {
    using ApplyFunc = void (*)(ImGuiStyle& style);

    const char* name;   ///< 設定ウィンドウに表示する名前
    ApplyFunc   apply;  ///< 既定値に戻した style にサイズと配色を設定する
  };

  /// <summary>
  /// 選択可能なテーマ一覧。index を EngineSettings.json に保存するため末尾にのみ追加する
  /// </summary>
  std::span<const ImGuiTheme> GetImGuiThemes();

  /// <summary>
  /// ImGuiStyle を既定値に戻してから index のテーマを適用する（前テーマのサイズ・色を残さない）
  /// </summary>
  void ApplyImGuiTheme(int index);

} // namespace Tako

#endif // _DEBUG
