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
    float GetDeltaTime() const { return deltaTime_; }
    float GetFPS() const { return fps_; }
    float GetDisplayFPS() const { return displayFPS_; }
    float GetGameTime() const { return gameTime_; }

  private: //非公開関数
    void UpdateDeltaTimeAndFPS();

    void UpdateGameTime();

  private: //メンバー変数
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point prevTime_;
    float                                 deltaTime_;
    float                                 fps_;

    float displayFPS_;

    float gameTime_;

    //1秒ごとにリセットする計測用の累積値
    float timeAccumulator_;
    int   frameCount_;
  };

} // namespace Tako