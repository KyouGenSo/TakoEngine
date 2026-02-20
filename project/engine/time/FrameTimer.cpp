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
    // システム時間の取得
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
    // deltaTime と FPS の更新
    UpdateDeltaTimeAndFPS();

    // gameTime の更新
    UpdateGameTime();
  }

  //----------------------------private----------------------------//

  void FrameTimer::UpdateDeltaTimeAndFPS()
  {
    // 現在の時間を取得
    auto nowTime = std::chrono::system_clock::now();

    // 前フレームからの経過時間を取得
    std::chrono::duration<float> elapsedTime = nowTime - prevTime_;
    float frameDelta = elapsedTime.count();

    // 前フレームの時間を更新
    prevTime_ = nowTime;

    // フレームごとの経過時間とカウントを蓄積
    timeAccumulator_ += frameDelta;
    frameCount_++;

    // 平均 deltaTime は累積時間をフレーム数で割った値
    deltaTime_ = timeAccumulator_ / frameCount_;
    // FPS はフレーム数を累積時間で割った値
    fps_ = static_cast<float>(frameCount_) / timeAccumulator_;

    // 蓄積時間が1秒以上になったら更新
    if (timeAccumulator_ >= 1.0f) {
      displayFPS_ = fps_;

      // 蓄積変数をリセット
      timeAccumulator_ = 0.0f;
      frameCount_ = 0;
    }
  }

  void FrameTimer::UpdateGameTime()
  {
    // システム時間の取得
    auto nowTime = std::chrono::system_clock::now();
    // ゲーム起動からの経過時間を取得
    std::chrono::duration<float> elapsedTime = nowTime - startTime_;
    gameTime_ = elapsedTime.count();
  }

} // namespace Tako