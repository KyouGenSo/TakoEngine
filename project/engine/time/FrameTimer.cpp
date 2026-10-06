#include "FrameTimer.h"

#include <algorithm>

namespace Tako {

  std::unique_ptr<FrameTimer> FrameTimer::instance_ = nullptr;

  FrameTimer* FrameTimer::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<FrameTimer>(Token{});
    }
    return instance_.get();
  }

  void FrameTimer::Initialize()
  {
    startTime_ = std::chrono::system_clock::now();
    prevTime_ = startTime_;

    deltaTime_ = 0.0f;
    fps_ = 0.0f;

    timeScale_ = 1.0f;
    timeScaleDuration_ = 0.0f;

    timeAccumulator_ = 0.0f;
    frameCount_ = 0;
  }

  void FrameTimer::Finalize()
  {
    instance_.reset();
  }

  void FrameTimer::Update()
  {
    const auto prevTime = prevTime_;
    UpdateDeltaTimeAndFPS();

    isFrozen_ = isPaused_ && !isStepRequested_;
    isStepRequested_ = false;
    if (isFrozen_) {
      // 停止していた時間をゲーム時間から除外する（再開時に時間が飛ばないように）
      startTime_ += prevTime_ - prevTime;
      return;
    }

    UpdateGameTime();

    UpdateTimeScaleDuration();
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

    // 直近1秒間の平均で deltaTime / FPS を算出。ブレークポイント等で止まった後に dt が跳ねないよう上限で抑える
    deltaTime_ = std::min(timeAccumulator_ / frameCount_, maxDeltaTime_);
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

  void FrameTimer::UpdateTimeScaleDuration()
  {
    if (timeScaleDuration_ <= 0.0f) return;

    // スケール後の値で減算すると timeScale 0 のとき永久停止するため、生の deltaTime を使う
    timeScaleDuration_ -= deltaTime_;
    if (timeScaleDuration_ <= 0.0f) {
      timeScale_ = 1.0f;
    }
  }

} // namespace Tako