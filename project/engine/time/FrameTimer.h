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

    FrameTimer() = default;
    ~FrameTimer() = default;

    friend struct std::default_delete<FrameTimer>;

  public:
    FrameTimer(const FrameTimer&) = delete;
    FrameTimer& operator=(const FrameTimer&) = delete;

    static FrameTimer* GetInstance();
    void Initialize();
    void Finalize();
    void Update();

    //----------------------------Getter----------------------------//
    float GetDeltaTime() const { return deltaTime_; }
    float GetFPS() const { return fps_; }
    // 1秒ごとに更新される表示用 FPS
    float GetDisplayFPS() const { return displayFPS_; }
    // ゲーム起動からの経過時間（秒）
    float GetGameTime() const { return gameTime_; }

  private:
    void UpdateDeltaTimeAndFPS();

    void UpdateGameTime();

  private:
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point prevTime_;
    float deltaTime_;
    float fps_;

    float displayFPS_;

    float gameTime_;

    // 1秒ごとにリセットする計測用の累積値
    float timeAccumulator_;
    int frameCount_;
  };

} // namespace Tako