#include "FrameTimer.h"

namespace Tako {

  std::unique_ptr<FrameTimer> FrameTimer::instance_ = nullptr;

  FrameTimer* FrameTimer::GetInstance()
  {
    if (!instance_) {
      instance_ = std::unique_ptr<FrameTimer>(new FrameTimer());
    }
    return instance_.get();
  }

  void FrameTimer::Initialize()
  {
    startTime_ = std::chrono::system_clock::now();
    prevTime_ = startTime_;

    deltaTime_ = 0.0f;
    fps_ = 0.0f;

    timeAccumulator_ = 0.0f;
    frameCount_ = 0;
  }

  void FrameTimer::Finalize()
  {
    instance_.reset();
  }

  void FrameTimer::Update()
  {
    UpdateDeltaTimeAndFPS();

    UpdateGameTime();
  }

  //----------------------------private----------------------------//

  void FrameTimer::UpdateDeltaTimeAndFPS()
  {
    auto nowTime = std::chrono::system_clock::now();

    std::chrono::duration<float> elapsedTime = nowTime - prevTime_;
    float frameDelta = elapsedTime.count();

    prevTime_ = nowTime;

    timeAccumulator_ += frameDelta;
    frameCount_++;

    // 直近1秒間の平均で deltaTime / FPS を算出
    deltaTime_ = timeAccumulator_ / frameCount_;
    fps_ = static_cast<float>(frameCount_) / timeAccumulator_;

    // 蓄積時間が1秒以上になったら表示用 FPS を更新し計測をリセット
    if (timeAccumulator_ >= 1.0f) {
      displayFPS_ = fps_;

      timeAccumulator_ = 0.0f;
      frameCount_ = 0;
    }
  }

  void FrameTimer::UpdateGameTime()
  {
    auto nowTime = std::chrono::system_clock::now();
    std::chrono::duration<float> elapsedTime = nowTime - startTime_;
    gameTime_ = elapsedTime.count();
  }

} // namespace Tako