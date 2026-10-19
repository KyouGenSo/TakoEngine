#pragma once

#ifdef _DEBUG

#include "imgui.h"

#include <format>
#include <functional>
#include <string>
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

  /// <summary>
  /// 内容が before から after に変わった編集の後も、未保存の変更が残っているか。未保存でなかった最後の内容を clean に覚え、編集で clean に戻れば残っていない
  /// </summary>
  /// <param name="wasEdited">この編集の前から未保存だったか</param>
  /// <returns>未保存の変更が残っていれば true</returns>
  inline bool IsStillEdited(bool wasEdited, const std::string& before, const std::string& after, std::string& clean) {
    // ファイルとは比べない。ゲームのコードが実行中に変えた値（表示の切り替えなど）は before に入っているので、未保存に数えない
    if (!wasEdited) {
      clean = before;
    }
    return after != clean;
  }

} // namespace Tako

#endif // _DEBUG
