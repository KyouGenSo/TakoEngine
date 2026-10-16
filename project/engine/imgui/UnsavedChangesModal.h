#pragma once

#ifdef _DEBUG

#include "imgui.h"

#include <format>
#include <functional>
#include <string_view>

namespace Tako {

  /// <summary>
  /// DrawUnsavedChangesModal で押されたボタン
  /// </summary>
  enum class UnsavedChoice {
    None,     ///< まだ押されていない（モーダルが閉じている場合も含む）
    Proceed,  ///< 保存に成功した、または破棄を選んだ。保留していた操作を実行してよい
    Cancel
  };

  /// <summary>
  /// 未保存の変更を失う操作の前に出す確認モーダル。開いている間は毎フレーム、同じ ID スタックで呼ぶ
  /// </summary>
  /// <param name="popupId">ポップアップの ID</param>
  /// <param name="openRequest">true ならこのフレームで開き、false に戻す</param>
  /// <param name="message">本文（複数行可）</param>
  /// <param name="action">続ける操作の名前。ボタン名（Save and Switch など）に使う</param>
  /// <param name="save">保存処理。空なら Save のボタンを出さない。false を返したら開いたままにする</param>
  /// <returns>押されたボタン</returns>
  inline UnsavedChoice DrawUnsavedChangesModal(const char* popupId, bool& openRequest, std::string_view message, std::string_view action,
                                               const std::function<bool()>& save = {}) {
    if (openRequest) {
      ImGui::OpenPopup(popupId);
      openRequest = false;
    }
    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      return UnsavedChoice::None;
    }

    UnsavedChoice choice = UnsavedChoice::None;
    ImGui::TextUnformatted(message.data(), message.data() + message.size());
    ImGui::Separator();
    if (save) {
      if (ImGui::Button(std::format("Save and {}", action).c_str()) && save()) {
        choice = UnsavedChoice::Proceed;
      }
      ImGui::SameLine();
    }
    if (ImGui::Button(std::format("Discard and {}", action).c_str())) {
      choice = UnsavedChoice::Proceed;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      choice = UnsavedChoice::Cancel;
    }
    if (choice != UnsavedChoice::None) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
    return choice;
  }

} // namespace Tako

#endif // _DEBUG
