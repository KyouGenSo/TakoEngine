#pragma once

#ifdef _DEBUG

#include "imgui.h"
#include "ShortcutTable.h"
#include "UnsavedChangesModal.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Tako {

  /// <summary>
  /// Undo / Redo のメニューかキーで選ばれた操作
  /// </summary>
  enum class HistoryCommand {
    None,
    Undo,
    Redo
  };

  /// <summary>
  /// 全エディタ共通の Undo / Redo のキー
  /// </summary>
  inline std::span<const ShortcutBinding<HistoryCommand>> HistoryShortcuts() {
    static constexpr ShortcutBinding<HistoryCommand> kShortcuts[] = {
      { ImGuiMod_Ctrl | ImGuiKey_Z,                  ImGuiInputFlags_None, "Undo the last edit in any editor (while typing in a text field, undoes the typing)", [](HistoryCommand& command) { command = HistoryCommand::Undo; } },
      { ImGuiMod_Ctrl | ImGuiKey_Y,                  ImGuiInputFlags_None, "Redo",                                                                               [](HistoryCommand& command) { command = HistoryCommand::Redo; } },
      { ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_None, "Redo",                                                                               [](HistoryCommand& command) { command = HistoryCommand::Redo; } },
    };
    return kShortcuts;
  }

  /// <summary>
  /// HistoryShortcuts のキーで選ばれた操作。フォーカスと文字の入力中かは呼び出し側で判定する
  /// </summary>
  inline HistoryCommand ReadHistoryShortcut() {
    HistoryCommand command = HistoryCommand::None;
    ProcessShortcuts(HistoryShortcuts(), command, ShortcutScope::Global);
    return command;
  }

  /// <summary>
  /// 全エディタで 1 本の Undo / Redo の順番。状態は各 EditHistory が持ち、ここは「どの履歴が手を積んだか」だけを積んだ順に覚える
  /// </summary>
  class UndoJournal {
  public: //構造体
    /// <summary>
    /// 順番に並ぶ履歴 1 つ分（EditHistory の基底）
    /// </summary>
    class Client {
    public: //メンバー関数
      Client(const char* name, std::function<void(bool isUndo)> apply, std::function<bool()> canApply = {})
        : name_(name), apply_(std::move(apply)), canApply_(std::move(canApply)) {}
      // UndoJournal が this を持つので複製しない
      Client(const Client&) = delete;
      Client& operator=(const Client&) = delete;

      //========================================
      //Getter
      //========================================
      const char* GetName() const { return name_; }

    protected: //非公開関数
      virtual ~Client() { Forget(); }

      /// <summary>
      /// 手を積んだ。全体の Redo の側は捨てる
      /// </summary>
      void NotifyCommit();

      /// <summary>
      /// 上限で一番古い手を捨てた
      /// </summary>
      void NotifyTrim();

      /// <summary>
      /// 履歴を捨てた。全体の順から自分の手を全部外す
      /// </summary>
      void Forget();

      virtual void ClearRedo() = 0;

    private: //メンバー変数
      friend class UndoJournal;

      const char*                      name_;      ///< メニューに出す名前
      std::function<void(bool isUndo)> apply_;     ///< 自分の履歴を 1 手戻す（isUndo）か、やり直す
      std::function<bool()>            canApply_;  ///< 省略可。false の間は、全体の順もこの履歴の手より先へ進まない
    };

  public: //メンバー関数
    /// <summary>
    /// 一番新しい手を積んだ履歴に 1 手戻させる（isUndo）か、最後に戻した履歴にやり直させる
    /// </summary>
    void Apply(bool isUndo) {
      if (!CanApply(isUndo)) {
        return;
      }
      std::vector<Client*>& from   = isUndo ? undo_ : redo_;
      Client*               client = from.back();
      // 相手が戻せなくても（対象が消えていた等）順は進める。同じ手で止まり続けないようにする
      // ponytail: 進行中の操作を相手が積んでから戻すと、その 1 手は Redo できない（NotifyCommit が redo_ を捨てる）。要るなら、戻す前に全部の履歴へ進行中の操作を積ませる
      from.pop_back();
      (isUndo ? redo_ : undo_).push_back(client);
      client->apply_(isUndo);
    }

    //========================================
    //Getter
    //========================================
    /// <summary>
    /// 今の順番（DebugUIManager が持つ）。null の間は、どの履歴も順番に積まない
    /// </summary>
    static UndoJournal*& Current() {
      static UndoJournal* current = nullptr;
      return current;
    }

    /// <summary>
    /// 次の Undo（isUndo）か Redo の相手（無ければ nullptr）
    /// </summary>
    const Client* Peek(bool isUndo) const {
      const std::vector<Client*>& from = isUndo ? undo_ : redo_;
      return from.empty() ? nullptr : from.back();
    }

    bool CanApply(bool isUndo) const {
      const Client* client = Peek(isUndo);
      return client && (!client->canApply_ || client->canApply_());
    }

  private: //メンバー変数
    std::vector<Client*> undo_;  ///< 手を積んだ順（後ろが新しい）
    std::vector<Client*> redo_;  ///< 戻した順（後ろが次にやり直す手）
  };

  inline void UndoJournal::Client::NotifyCommit() {
    UndoJournal* journal = Current();
    if (!journal) {
      return;
    }
    for (Client* client : journal->redo_) {
      client->ClearRedo();
    }
    journal->redo_.clear();
    journal->undo_.push_back(this);
  }

  inline void UndoJournal::Client::NotifyTrim() {
    if (UndoJournal* journal = Current()) {
      if (const auto oldest = std::ranges::find(journal->undo_, this); oldest != journal->undo_.end()) {
        journal->undo_.erase(oldest);
      }
    }
  }

  inline void UndoJournal::Client::Forget() {
    if (UndoJournal* journal = Current()) {
      std::erase(journal->undo_, this);
      std::erase(journal->redo_, this);
    }
  }

  /// <summary>
  /// Undo / Redo の履歴。編集が始まったら Record で前の状態を覚え、操作を終えたら Commit で 1 手として積む。積んだ手は UndoJournal の順に入る
  /// </summary>
  /// <typeparam name="State">戻す状態。== が false なら別の手とみなす</typeparam>
  template <class State>
  class EditHistory : public UndoJournal::Client {
  public: //定数
    static constexpr size_t kMaxSteps = 100;  ///< 戻れる手数

  public: //メンバー関数
    using UndoJournal::Client::Client;

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
          NotifyTrim();
        }
        redo_.clear();
        NotifyCommit();
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
      Forget();
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
    void ClearRedo() override { redo_.clear(); }

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
      std::function<void(const State&)> restore;     ///< Undo / Redo で state に戻す
      std::function<uint64_t()>         hash;        ///< 省略可。渡すと、ハッシュが変わったときだけ serialize する
      std::function<bool()>             canRestore;  ///< 省略可。false の間は Undo / Redo で戻さない
    };

  public: //メンバー関数
    /// <param name="name">Undo / Redo のメニューに出す名前</param>
    EditTracker(const char* name, Callbacks callbacks)
      : callbacks_(std::move(callbacks)), history_(name, [this](bool isUndo) { Restore(isUndo); }, callbacks_.canRestore) {}

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
    /// UI の区間の終わり。ウィンドウの End の前に毎フレーム呼ぶ。操作を終えたら前後を比べ、変わっていれば未保存の印を付けて Undo の 1 手にする
    /// </summary>
    void EndSection() {
      if (gate_.EndOperation()) {
        FinishOperation();
      }
    }

    /// <summary>
    /// 保存した。次の編集から、今の内容を未保存でない内容として比べる
    /// </summary>
    void MarkSaved() { isEdited_ = false; }

    /// <summary>
    /// UI の区間の外でコードが内容を変えたときに、Undo の 1 手として積み、未保存の印を付ける
    /// </summary>
    /// <param name="before">変える前に serialize した内容</param>
    void RecordExternalChange(const State& before) {
      State after = callbacks_.serialize();
      RecordChange(before, after);
      current_ = std::move(after);
      if (callbacks_.hash) {
        currentHash_ = startHash_ = callbacks_.hash();
      }
    }

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

    /// <summary>
    /// 1 手戻す（isUndo）か、やり直す。UndoJournal が全体の順に沿って呼ぶ
    /// </summary>
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

  /// <summary>
  /// UndoJournal が 2 つの履歴の手を、積んだ順に戻すかを確かめる（起動時に 1 回）
  /// </summary>
  inline void RunUndoJournalSelfTest() {
    UndoJournal        journal;
    UndoJournal* const previous = std::exchange(UndoJournal::Current(), &journal);

    int              a = 0;
    int              b = 0;
    EditHistory<int> historyA{ "A", [&](bool isUndo) { if (const std::optional<int> state = isUndo ? historyA.Undo(a) : historyA.Redo(a)) { a = *state; } } };
    EditHistory<int> historyB{ "B", [&](bool isUndo) { if (const std::optional<int> state = isUndo ? historyB.Undo(b) : historyB.Redo(b)) { b = *state; } } };
    const auto       edit = [](EditHistory<int>& history, int& value, int next) {
      history.Record(value);
      value = next;
      history.Commit(value);
    };

    edit(historyA, a, 1);
    edit(historyB, b, 1);
    edit(historyA, a, 2);

    // 新しい順に、積んだ履歴が戻す
    journal.Apply(true);
    assert(a == 1 && b == 1);
    journal.Apply(true);
    assert(a == 1 && b == 0);
    journal.Apply(false);
    assert(a == 1 && b == 1);

    // 新しい手で、別の履歴の Redo も捨てる
    edit(historyB, b, 5);
    assert(!journal.CanApply(false) && !historyA.CanRedo());

    // 捨てた履歴の手は、順からも消える
    historyB.Clear();
    journal.Apply(true);
    assert(a == 0 && b == 5);
    assert(!journal.CanApply(true));

    UndoJournal::Current() = previous;
  }

} // namespace Tako

#endif // _DEBUG
