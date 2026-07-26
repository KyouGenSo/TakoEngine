#pragma once
#include <chrono>
#include <memory>

namespace Tako {

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
    float GetDeltaTime() const { return deltaTime_ * timeScale_; }
    float GetUnscaledDeltaTime() const { return deltaTime_; }
    float GetTimeScale() const { return timeScale_; }
    float GetFPS() const { return fps_; }
    float GetDisplayFPS() const { return displayFPS_; }
    float GetGameTime() const { return gameTime_; }

    //=========================
    //Setter
    //=========================
    void SetTimeScale(float scale) { timeScale_ = scale; timeScaleDuration_ = 0.0f; }
    //指定秒数だけスケールを適用し、経過後 1.0 に自動復帰する。duration が 0 以下なら何もしない(永久停止防止)
    void SetTimeScaleForDuration(float scale, float duration) { if (duration <= 0.0f) return; timeScale_ = scale; timeScaleDuration_ = duration; }

  private: //非公開関数
    void UpdateDeltaTimeAndFPS();

    void UpdateGameTime();

    void UpdateTimeScaleDuration();

  private: //メンバー変数
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point prevTime_;
    float                                 deltaTime_;
    float                                 fps_;

    float displayFPS_;

    float gameTime_;

    //残り秒数が0以下の間はスケールを維持し続ける
    float timeScale_;
    float timeScaleDuration_;

    //1秒ごとにリセットする計測用の累積値
    float timeAccumulator_;
    int   frameCount_;
  };

} // namespace Tako