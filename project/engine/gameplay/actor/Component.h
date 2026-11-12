#pragma once
#include "TObject.h"

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Tako {

  class Actor;

  /// <summary>
  /// Component::SetTimer が返す番号。0 は無効
  /// </summary>
  using TimerId = uint64_t;

  /// <summary>
  /// Actor に付ける機能の単位（UE の UActorComponent）。C++ のスクリプトは Script を継承し、ScriptComponent に入れる
  /// </summary>
  TCLASS()
  class Component : public TObject {
    GENERATED_BODY()

  public: //メンバー関数
    ~Component() override = default;

    /// <summary>
    /// BeginPlay の前に 1 回呼ばれる。同時にシーンへ入る全 Actor の Awake が済んでから、BeginPlay が始まる。
    /// 他の BeginPlay から名前や参照で引かれる物は、ここで用意する
    /// </summary>
    virtual void Awake() {}

    /// <summary>
    /// Play が始まったとき、または Play 中に足されたときに 1 回呼ばれる。
    /// 呼ぶ順は TCLASS(Order = n) の小さい順（同じなら Hierarchy の並び、同じ Actor の中ではコンポーネントの並び）。Awake / Tick / LateTick も同じ
    /// </summary>
    virtual void BeginPlay() {}

    /// <summary>
    /// Play 中だけ毎フレーム呼ばれる。Tick の中で足されたコンポーネントは、次のフレームから呼ばれる
    /// </summary>
    virtual void Tick([[maybe_unused]] float deltaTime) {}

    /// <summary>
    /// 全 Actor の Tick の後、シーンの Update（当たり判定）の前に呼ばれる。他の Actor が動いた後の位置を使う処理を書く
    /// </summary>
    virtual void LateTick([[maybe_unused]] float deltaTime) {}

    /// <summary>
    /// true を返すと、シーンのポーズ中（BaseScene::SetPaused）も Tick と LateTick が呼ばれる。ポーズのメニューなどに使う
    /// </summary>
    virtual bool TickWhilePaused() const { return false; }

    /// <summary>
    /// Play が終わるとき、または外される・消されるときに呼ばれる（BeginPlay を呼んだものだけ）
    /// </summary>
    virtual void EndPlay() {}

    /// <summary>
    /// 毎フレーム（Edit 中・一時停止中も）呼ばれる。描画物の登録を書く
    /// </summary>
    virtual void Draw() {}

    /// <summary>
    /// Actor の Active（自分か親のどれか）が切り替わった次のフレームの頭に呼ばれる（Edit 中も）。
    /// オフの間は Tick / LateTick / Draw が呼ばれないので、それ以外で動いている物（当たり判定・音・エミッター）をここで止める
    /// </summary>
    virtual void OnActiveChanged([[maybe_unused]] bool active) {}

    /// <summary>
    /// 同じ Actor の他のコンポーネントが、これから外される（Edit 中も）。そのコンポーネントの物を指しているなら、ここで離す
    /// </summary>
    virtual void OnSiblingRemoved([[maybe_unused]] Component* removed) {}

    /// <summary>
    /// 同じ Actor の ColliderComponent が当たり始めた。other は相手の Actor（Actor でないコライダーなら nullptr）
    /// </summary>
    virtual void OnCollisionEnter([[maybe_unused]] Actor* other) {}

    virtual void OnCollisionStay([[maybe_unused]] Actor* other) {}

    virtual void OnCollisionExit([[maybe_unused]] Actor* other) {}

    /// <summary>
    /// seconds 秒後に callback を呼ぶ。loops なら seconds 秒ごとに呼び続ける。
    /// Tick と同じ条件で進み（ポーズ中と、Actor がオフの間は止まる）、このコンポーネントが消えると一緒に消える
    /// </summary>
    /// <returns>ClearTimer に渡す番号</returns>
    TimerId SetTimer(float seconds, std::function<void()> callback, bool loops = false)
    {
      timers_.push_back({ nextTimerId_, seconds, seconds, loops, std::move(callback) });
      return nextTimerId_++;
    }

    /// <summary>
    /// SetTimer を取り消す。タイマーの関数の中からも呼べる
    /// </summary>
    void ClearTimer(TimerId id)
    {
      for (Timer& timer : timers_) {
        if (timer.id == id) { timer.id = 0; }
      }
    }

    /// <summary>
    /// BaseScene::SendEvent で送られた name を handler で受け取る。このコンポーネントが消えると、受け取りも終わる
    /// </summary>
    void Subscribe(std::string name, std::function<void(const std::any& payload)> handler)
    {
      eventHandlers_.emplace_back(std::move(name), std::move(handler));
    }

    /// <summary>
    /// タイマーを進め、時間の来た物を呼ぶ。シーンが Tick の前に呼ぶ
    /// </summary>
    void AdvanceTimers(float deltaTime);

    /// <summary>
    /// name を受け取る関数を呼ぶ。BaseScene::SendEvent が呼ぶ
    /// </summary>
    void HandleEvent(std::string_view name, const std::any& payload);

    //=====================================
    //Getter
    //=====================================
    Actor* GetOwner() const { return owner_; }

    const std::string& GetName() const { return name_; }

  protected: //メンバー関数
    // 直接は作らない（Add Component の一覧にも出ない）
    Component() = default;

  private: //構造体
    struct Timer {
      TimerId               id;         ///< 0 は取り消し済み（次の AdvanceTimers で外す）
      float                 interval;
      float                 remaining;
      bool                  loops;
      std::function<void()> callback;
    };

  private: //メンバー変数
    friend class Actor;
    friend class ScriptComponent;

    Actor*                                                                     owner_       = nullptr;
    std::string                                                                name_;                   ///< Actor の中で一意。保存ファイルのキー
    std::vector<Timer>                                                         timers_;
    TimerId                                                                    nextTimerId_ = 1;
    std::vector<std::pair<std::string, std::function<void(const std::any&)>>> eventHandlers_;
  };

  inline void Component::AdvanceTimers(float deltaTime)
  {
    // 呼んだ関数が SetTimer / ClearTimer しても壊れないよう、添字で回して最後にまとめて外す。この中で足された物は次のフレームから進む
    const size_t count = timers_.size();
    for (size_t i = 0; i < count; ++i) {
      if (timers_[i].id == 0) {
        continue;
      }
      timers_[i].remaining -= deltaTime;
      if (timers_[i].remaining > 0.0f) {
        continue;
      }
      const std::function<void()> callback = timers_[i].callback;
      if (timers_[i].loops) {
        timers_[i].remaining += timers_[i].interval;
      } else {
        timers_[i].id = 0;
      }
      callback();
    }
    std::erase_if(timers_, [](const Timer& timer) { return timer.id == 0; });
  }

  inline void Component::HandleEvent(std::string_view name, const std::any& payload)
  {
    // 呼んだ関数が Subscribe しても壊れないよう、添字で回して関数は写してから呼ぶ
    for (size_t i = 0; i < eventHandlers_.size(); ++i) {
      if (eventHandlers_[i].first == name) {
        const std::function<void(const std::any&)> handler = eventHandlers_[i].second;
        handler(payload);
      }
    }
  }

} // namespace Tako
