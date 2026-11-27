#pragma once

#ifdef TAKO_EDITOR

#include "EditorUI.h"
#include "imgui.h"

#include <functional>
#include <string>
#include <utility>

namespace Tako {

  /// <summary>
  /// 取り消しにくい操作（削除など）を、確認モーダルの返答まで保留する
  /// </summary>
  class ConfirmPrompt {
  public: //メンバー関数
    /// <summary>
    /// action を保留して、次の Draw で確認を出す。action が空なら、本文を知らせるだけにする
    /// </summary>
    /// <param name="message">本文（複数行可）</param>
    /// <param name="actionLabel">実行するボタンの名前</param>
    void Request(std::string message, std::string actionLabel, std::function<void()> action) {
      message_     = std::move(message);
      actionLabel_ = std::move(actionLabel);
      action_      = std::move(action);
      isRequested_ = true;
    }

    /// <summary>
    /// 確認モーダル。開いている間は毎フレーム、同じ ID スタックで呼ぶ。実行を選んだら保留した操作を実行する
    /// </summary>
    void Draw(const char* popupId) {
      if (isRequested_) {
        ImGui::OpenPopup(popupId);
        isRequested_ = false;
      }
      if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
      }

      ImGui::TextUnformatted(message_.c_str());
      ImGui::Separator();
      bool isConfirmed = false;
      if (action_) {
        isConfirmed = UI::DangerButton(actionLabel_.c_str());
        ImGui::SameLine();
      }
      const bool isClosed = ImGui::Button(action_ ? "Cancel" : "Close") || ImGui::IsKeyPressed(ImGuiKey_Escape);
      if (isConfirmed || isClosed) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();

      if (isConfirmed || isClosed) {
        // 実行中に次の確認を予約できるよう、先に取り出す
        const std::function<void()> action = std::exchange(action_, nullptr);
        if (isConfirmed && action) {
          action();
        }
      }
    }

  private: //メンバー変数
    std::string           message_;
    std::string           actionLabel_;
    std::function<void()> action_;
    bool                  isRequested_ = false;
  };

} // namespace Tako

#endif // TAKO_EDITOR
