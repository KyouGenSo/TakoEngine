#include "FrameTimer.h"

FrameTimer* FrameTimer::instance_ = nullptr;

FrameTimer* FrameTimer::GetInstance()
{
  if (instance_ == nullptr)
  {
    instance_ = new FrameTimer();
  }
  return instance_;
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
  if (instance_ != nullptr)
  {
    delete instance_;
    instance_ = nullptr;
  }
}

void FrameTimer::Update()
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

  // 蓄積時間が1秒以上になったら更新
  if (timeAccumulator_ >= 1.0f)
  {
    // 平均deltaTimeは累積時間をフレーム数で割った値
    deltaTime_ = timeAccumulator_ / frameCount_;
    // FPSはフレーム数を累積時間で割った値
    fps_ = static_cast<float>(frameCount_) / timeAccumulator_;

    // 蓄積変数をリセット
    timeAccumulator_ = 0.0f;
    frameCount_ = 0;
  }
}