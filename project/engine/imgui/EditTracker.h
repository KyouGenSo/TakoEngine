#pragma once

#ifdef _DEBUG

#include "imgui.h"
#include "UnsavedChangesModal.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Tako {

  /// <summary>
  /// Undo / Redo のボタンかキーで選ばれた操作
  /// </summary>
  enum class HistoryCommand {
    None,
    Undo,
    Redo
  };

  /// <summary>
  /// Undo / Redo のボタン（できないときは無効）。ツールバーの行に置く
  /// </summary>
  inline HistoryCommand DrawHistoryButtons(bool canUndo, bool canRedo) {
    HistoryCommand command = HistoryCommand::None;
    ImGui::BeginDisabled(!canUndo);
    if (ImGui::Button("Undo")) {
      command = HistoryCommand::Undo;
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Ctrl+Z");
    ImGui::SameLine();
    ImGui::BeginDisabled(!canRedo);
    if (ImGui::Button("Redo")) {
      command = HistoryCommand::Redo;
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Ctrl+Y / Ctrl+Shift+Z");
    return command;
  }

  /// <summary>
  /// Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z。ImGui::Shortcut なのでフォーカス中のウィンドウにだけ届き、入力欄の編集中は入力欄の Undo が優先される。
  /// ウィンドウの Begin と End の間で呼ぶ
  /// </summary>
  inline HistoryCommand ReadHistoryShortcut() {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) {
      return HistoryCommand::Undo;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
      return HistoryCommand::Redo;
    }
    return HistoryCommand::None;
  }

  /// <summary>
  /// Undo / Redo の履歴。編集が始まったら Record で前の状態を覚え、操作を終えたら Commit で 1 手として積む
  /// </summary>
  /// <typeparam name="State">戻す状態。== が false なら別の手とみなす</typeparam>
  template <class State>
  class EditHistory {
  public: //定数
    static constexpr size_t kMaxSteps = 100;  ///< 戻れる手数

  public: //メンバー関数
    /// <summary>
    /// 進行中の操作の前の状態を覚える。すでに覚えていれば何もしない（ドラッグ中の毎フレームの変化を 1 手にまとめる）
    /// </summary>
    void Record(const State& before) {
      if (!pending_) {
        pending_ = before;
      }
    }

    /// <summary>
    /// 進行中の操作を 1 手として積み、Redo の履歴を捨てる。current と同じなら（ドラッグして元の位置へ戻した場合など）積まない
    /// </summary>
    void Commit(const State& current) {
      if (!pending_) {
        return;
      }
      if (!(*pending_ == current)) {
        undo_.push_back(std::move(*pending_));
        if (undo_.size() > kMaxSteps) {
          undo_.erase(undo_.begin());
        }
        redo_.clear();
      }
      pending_.reset();
    }

    /// <summary>
    /// 進行中の操作を積んでから 1 手戻す。current は Redo の側に積む
    /// </summary>
    /// <returns>戻す状態（戻せなければ nullopt）</returns>
    std::optional<State> Undo(const State& current) {
      Commit(current);
      return Move(undo_, redo_, current);
    }

    /// <summary>
    /// 進行中の操作を積んでから 1 手やり直す。current は Undo の側に積む
    /// </summary>
    /// <returns>戻す状態（やり直せなければ nullopt）</returns>
    std::optional<State> Redo(const State& current) {
      Commit(current);
      return Move(redo_, undo_, current);
    }

    void Clear() {
      undo_.clear();
      redo_.clear();
      pending_.reset();
    }

    //========================================
    //Getter
    //========================================
    /// <summary>
    /// 次の Undo で戻す状態（無ければ nullptr）。進行中の操作は含まないので、Commit してから呼ぶ
    /// </summary>
    const State* PeekUndo() const { return undo_.empty() ? nullptr : &undo_.back(); }

    /// <summary>
    /// 次の Redo で戻す状態（無ければ nullptr）
    /// </summary>
    const State* PeekRedo() const { return redo_.empty() ? nullptr : &redo_.back(); }

    /// <summary>
    /// 進行中の操作も Undo で 1 手として戻せる
    /// </summary>
    bool CanUndo() const { return pending_.has_value() || !undo_.empty(); }

    bool CanRedo() const { return !redo_.empty(); }

    /// <summary>
    /// 進行中の操作（Record して Commit していない）があるか
    /// </summary>
    bool IsRecording() const { return pending_.has_value(); }

  private: //非公開関数
    static std::optional<State> Move(std::vector<State>& from, std::vector<State>& to, const State& current) {
      if (from.empty()) {
        return std::nullopt;
      }
      State state = std::move(from.back());
      from.pop_back();
      to.push_back(current);
      return state;
    }

  private: //メンバー変数
    std::vector<State>   undo_;
    std::vector<State>   redo_;
    std::optional<State> pending_;  ///< 進行中の操作の前の状態
  };

  /// <summary>
  /// 編集を比べる 1 回の操作の始まりと終わりを決める。ウィンドウ（子ウィンドウとそこから開いたポップアップを含む）への入力
  /// （マウスのボタン・ホイール・キー・文字）で始まり、アイテムの操作とマウスのボタンを離したら終わる。
  /// 操作の前後だけ比べればよいので、何もしていないフレームは保存内容を作らない
  /// </summary>
  class EditGate {
  public: //メンバー関数
    /// <summary>
    /// 操作が始まったか。ウィンドウの Begin の直後に毎フレーム呼び、true なら操作の前の内容を取る
    /// </summary>
    bool BeginOperation() {
      // ponytail: 入力の無いフレームに区間の中で値を変えるコード（時間で変わる UI など）を入れたら、この判定は使えない
      if (isOperating_ || !HasWindowInput()) {
        return false;
      }
      isOperating_ = true;
      return true;
    }

    /// <summary>
    /// 操作を終えたか。区間の終わりに毎フレーム呼び、true なら操作の後の内容を取って比べる
    /// </summary>
    bool EndOperation() {
      // IsItemDeactivatedAfterEdit で確定する入力欄は、別のウィンドウをクリックしたフレームに値を書くので、ボタンを離すまで待つ
      if (!isOperating_ || ImGui::IsAnyItemActive() || ImGui::IsAnyMouseDown()) {
        return false;
      }
      isOperating_ = false;
      return true;
    }

    /// <summary>
    /// 進行中の操作を捨てる（読み込みで内容を置き換えたとき）
    /// </summary>
    void Cancel() { isOperating_ = false; }

    //========================================
    //Getter
    //========================================
    bool IsOperating() const { return isOperating_; }

  private: //非公開関数
    /// <summary>
    /// このフレームに、今のウィンドウへの入力があったか
    /// </summary>
    static bool HasWindowInput() {
      const ImGuiIO& io = ImGui::GetIO();
      if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f) {
          return true;
        }
        for (int button = 0; button < ImGuiMouseButton_COUNT; ++button) {
          // 離したときも含める（別のウィンドウからのドラッグ＆ドロップは、このウィンドウの上で離したフレームに届く）
          if (ImGui::IsMouseClicked(button) || ImGui::IsMouseReleased(button)) {
            return true;
          }
        }
      }
      if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return false;
      }
      if (!io.InputQueueCharacters.empty()) {
        return true;
      }
      for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key)) || ImGui::IsKeyReleased(static_cast<ImGuiKey>(key))) {
          return true;
        }
      }
      return false;
    }

  private: //メンバー変数
    bool isOperating_ = false;
  };

  /// <summary>
  /// 1 つの編集対象の保存内容を、UI の区間での 1 回の操作の前後で比べて編集を見つける。未保存の印と Undo / Redo の履歴を持つ
  /// </summary>
  /// <typeparam name="State">保存内容。== で比べる</typeparam>
  template <class State = std::string>
  class EditTracker {
  public: //構造体
    /// <summary>
    /// 編集対象の読み書き
    /// </summary>
    struct Callbacks {
      std::function<State()>            serialize;
      std::function<void(const State&)> restore;  ///< Undo / Redo で state に戻す
      std::function<uint64_t()>         hash;     ///< 省略可。渡すと、ハッシュが変わったときだけ serialize する
    };

  public: //メンバー関数
    explicit EditTracker(Callbacks callbacks) : callbacks_(std::move(callbacks)) {}

    /// <summary>
    /// UI の区間の始まり。ウィンドウの Begin の直後に毎フレーム呼ぶ。操作が始まったフレームだけ、操作の前の内容を取る
    /// </summary>
    void BeginSection() {
      if (!gate_.BeginOperation()) {
        return;
      }
      if (!callbacks_.hash) {
        current_ = callbacks_.serialize();
        return;
      }
      // 前の操作の後にゲームのコードが内容を変えていなければ、そのときの内容を使う
      startHash_ = callbacks_.hash();
      if (!current_ || startHash_ != currentHash_) {
        current_     = callbacks_.serialize();
        currentHash_ = startHash_;
      }
    }

    /// <summary>
    /// UI の区間の終わり。ウィンドウの End の前に毎フレーム呼ぶ。操作を終えたら前後を比べ、変わっていれば未保存の印を付けて Undo の 1 手にする。
    /// Ctrl+Z / Ctrl+Y もここで受ける
    /// </summary>
    void EndSection() {
      if (gate_.EndOperation()) {
        FinishOperation();
      }
      switch (ReadHistoryShortcut()) {
      case HistoryCommand::Undo: Undo(); break;
      case HistoryCommand::Redo: Redo(); break;
      default: break;
      }
    }

    /// <summary>
    /// Undo / Redo のボタン。ツールバーの行に置く
    /// </summary>
    void DrawButtons() {
      switch (DrawHistoryButtons(CanUndo(), CanRedo())) {
      case HistoryCommand::Undo: Undo(); break;
      case HistoryCommand::Redo: Redo(); break;
      default: break;
      }
    }

    void Undo() { Restore(true); }
    void Redo() { Restore(false); }

    /// <summary>
    /// 保存した。次の編集から、今の内容を未保存でない内容として比べる
    /// </summary>
    void MarkSaved() { isEdited_ = false; }

    /// <summary>
    /// 読み込んだ・切り替えた。未保存の印と履歴を捨て、進行中の操作も比べない
    /// </summary>
    void Reset() {
      isEdited_ = false;
      history_.Clear();
      current_.reset();
      gate_.Cancel();
    }

    //========================================
    //Getter
    //========================================
    bool IsEdited() const { return isEdited_; }
    bool CanUndo() const { return history_.CanUndo(); }
    bool CanRedo() const { return history_.CanRedo(); }

  private: //非公開関数
    void FinishOperation() {
      // 操作の途中で Reset した（読み込んだ）場合は比べない
      if (!current_) {
        return;
      }
      std::optional<uint64_t> hash;
      if (callbacks_.hash) {
        hash = callbacks_.hash();
        if (*hash == startHash_) {
          return;
        }
      }
      State after = callbacks_.serialize();
      RecordChange(*current_, after);
      current_ = std::move(after);
      if (hash) {
        currentHash_ = *hash;
      }
    }

    /// <summary>
    /// before から after への変化を Undo の 1 手にし、未保存の印を付け直す（同じなら何もしない）
    /// </summary>
    void RecordChange(const State& before, const State& after) {
      if (before == after) {
        return;
      }
      history_.Record(before);
      history_.Commit(after);
      isEdited_ = IsStillEdited(isEdited_, before, after, clean_);
    }

    void Restore(bool isUndo) {
      // 操作の途中で戻す場合は、そこまでの編集を 1 手にしてから戻す
      const State before = callbacks_.serialize();
      if (gate_.IsOperating() && current_) {
        RecordChange(*current_, before);
      }
      const std::optional<State> state = isUndo ? history_.Undo(before) : history_.Redo(before);
      if (!state) {
        return;
      }
      callbacks_.restore(*state);
      State after = callbacks_.serialize();
      isEdited_   = IsStillEdited(isEdited_, before, after, clean_);
      // 操作の途中で戻した場合は、戻した内容から比べ直す
      current_ = std::move(after);
      if (callbacks_.hash) {
        currentHash_ = startHash_ = callbacks_.hash();
      }
    }

  private: //メンバー変数
    Callbacks callbacks_;

    //編集の検出
    EditGate             gate_;
    std::optional<State> current_;          ///< 最後に serialize した内容。操作の間は操作の前の内容
    uint64_t             currentHash_ = 0;  ///< current_ のハッシュ（hash を渡したときだけ使う）
    uint64_t             startHash_   = 0;  ///< 操作の前のハッシュ

    //未保存の印と履歴
    bool               isEdited_ = false;
    State              clean_;             ///< isEdited_ が付く直前の内容。編集でここへ戻れば未保存を外す
    EditHistory<State> history_;
  };

} // namespace Tako

#endif // _DEBUG
