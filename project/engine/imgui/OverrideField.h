#pragma once

#ifdef _DEBUG

#include "imgui.h"

#include <optional>

namespace Tako {

  /// <summary>
  /// 「上書きする」のチェックと入力欄の 1 行。チェックを外した間は、引き継ぐ値を灰色で出す
  /// </summary>
  /// <param name="value">上書きする値（nullopt なら引き継ぐ）</param>
  /// <param name="inherited">上書きしないときの値（チェックを入れたときの初期値にもする）</param>
  /// <param name="widget">bool(const char* label, T& value)。値を変えたら true</param>
  /// <returns>value を変えたら true</returns>
  template <class T, class Widget>
  bool OverrideField(const char* label, std::optional<T>& value, const T& inherited, Widget&& widget) {
    ImGui::PushID(label);
    bool changed = false;
    bool enabled = value.has_value();
    if (ImGui::Checkbox("##Override", &enabled)) {
      value   = enabled ? std::optional<T>(inherited) : std::nullopt;
      changed = true;
    }
    ImGui::SetItemTooltip("Checked: use this value. Unchecked: use the inherited value (gray)");
    ImGui::SameLine();
    ImGui::BeginDisabled(!enabled);
    T shown = value.value_or(inherited);
    if (widget(label, shown) && enabled) {
      value   = shown;
      changed = true;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
  }

} // namespace Tako

#endif // _DEBUG
