#pragma once
#include <chrono>

class FrameTimer
{
private: // シングルトン設定
  // インスタンス
  static FrameTimer* instance_;
  FrameTimer() = default;
  FrameTimer(const FrameTimer&) = delete;
  FrameTimer& operator=(const FrameTimer&) = delete;
  ~FrameTimer() = default;

public:
  // インスタンスの取得
  static FrameTimer* GetInstance();
  // 初期化
  void Initialize();
  // 終了処理
  void Finalize();
  // 更新
  void Update();

  //----------------------------Getter----------------------------//
  // deltaTimeの取得
  float GetDeltaTime() const { return deltaTime_; }
  // 実際のFPSの取得
  float GetFPS() const { return fps_; }
  // 表示用FPSの取得
  float GetDisplayFPS() const { return displayFPS_; }
  // ゲーム起動からの経過時間の取得
  float GetGameTime() const { return gameTime_; }

private:
  // deltaTimeとFPSの更新
  void UpdateDeltaTimeAndFPS();

  // gameTimeの更新
  void UpdateGameTime();

private:
  // システム時間
  std::chrono::system_clock::time_point startTime_;
  // 前フレームの時間
  std::chrono::system_clock::time_point prevTime_;
  // deltaTime
  float deltaTime_;
  // FPS
  float fps_;

  // 表示用FPS
  float displayFPS_;

  // ゲーム起動からの経過時間
  float gameTime_;

  // 1秒間の計測用
  float timeAccumulator_;
  int frameCount_;
};