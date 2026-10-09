#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace Tako {

  /// <summary>
  /// 60fps の 1 フレームあたりで調整された値を、実際の deltaTime に合わせて換算する
  /// </summary>
  namespace FrameRate {

    inline constexpr float kReference = 60.0f;

    /// <summary>
    /// deltaTime が基準フレーム何枚分かを返す（60fps なら 1）
    /// </summary>
    inline float Frames(float deltaTime) { return deltaTime * kReference; }

    /// <summary>
    /// 毎フレーム適用していた補間係数を、同じ速さで収束する係数に換算する
    /// </summary>
    inline float LerpFactor(float perFrameT, float deltaTime) {
      return 1.0f - std::pow(1.0f - std::clamp(perFrameT, 0.0f, 1.0f), Frames(deltaTime));
    }

    /// <summary>
    /// 毎フレーム乗算していた減衰率を換算する
    /// </summary>
    inline float Damping(float perFrameRate, float deltaTime) { return std::pow(perFrameRate, Frames(deltaTime)); }

  } // namespace FrameRate

  /// <summary>
  /// フレーム時間管理クラス。デルタタイム、FPS 計測、ゲーム経過時間の管理を行う
  /// </summary>
  class FrameTimer
  {
  private: // シングルトン設定
    static std::unique_ptr<FrameTimer> instance_;

    struct Token {};  ///< 外部からの直接生成を防ぐ生成キー
    ~FrameTimer() = default;

    friend struct std::default_delete<FrameTimer>;

  public:
    explicit FrameTimer(Token) {}
    FrameTimer(const FrameTimer&) = delete;
    FrameTimer& operator=(const FrameTimer&) = delete;

  public: //メンバー関数
    static FrameTimer* GetInstance();
    void Initialize();
    void Finalize();
    void Update();

    //=========================
    //Getter
    //=========================
    /// <summary>
    /// スケール済みの deltaTime（凍結中は 0）
    /// </summary>
    float GetDeltaTime() const { return isFrozen_ ? 0.0f : deltaTime_ * timeScale_; }
    float GetUnscaledDeltaTime() const { return deltaTime_; }
    float GetTimeScale() const { return timeScale_; }
    float GetFPS() const { return fps_; }
    float GetDisplayFPS() const { return displayFPS_; }
    float GetGameTime() const { return gameTime_; }
    float GetMaxDeltaTime() const { return maxDeltaTime_; }
    bool IsPaused() const { return isPaused_; }

    /// <summary>
    /// このフレームのゲーム更新を止めるか（一時停止中かつ Step 要求なし。Update で決まる）
    /// </summary>
    bool IsFrozen() const { return isFrozen_; }

    //=========================
    //Setter
    //=========================
    void SetTimeScale(float scale) { timeScale_ = scale; timeScaleDuration_ = 0.0f; }
    //指定秒数だけスケールを適用し、経過後 1.0 に自動復帰する。duration が 0 以下なら何もしない(永久停止防止)
    void SetTimeScaleForDuration(float scale, float duration) { if (duration <= 0.0f) return; timeScale_ = scale; timeScaleDuration_ = duration; }
    void SetMaxDeltaTime(float seconds) { maxDeltaTime_ = seconds; }

    /// <summary>
    /// 一時停止中はゲーム時間・deltaTime・時間スケールの残り時間を止める（FPS 計測は継続）
    /// </summary>
    void SetPaused(bool paused) { isPaused_ = paused; }

    /// <summary>
    /// 一時停止中に次の Update の 1 フレームだけ進める
    /// </summary>
    void RequestStep() { isStepRequested_ = true; }

  private: //非公開関数
    void UpdateDeltaTimeAndFPS();

    void UpdateGameTime();

    void UpdateTimeScaleDuration();

  private: //メンバー変数
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point prevTime_;
    float                                 deltaTime_;
    float                                 fps_;
    float                                 maxDeltaTime_ = 0.1f;  ///< 秒。deltaTime_ の上限（FPS 計測には掛けない）

    float displayFPS_;

    float gameTime_;

    //残り秒数が0以下の間はスケールを維持し続ける
    float timeScale_;
    float timeScaleDuration_;

    //1秒ごとにリセットする計測用の累積値
    float timeAccumulator_;
    int   frameCount_;

    //一時停止
    bool isPaused_        = false;
    bool isStepRequested_ = false;
    bool isFrozen_        = false;
  };

} // namespace Tako