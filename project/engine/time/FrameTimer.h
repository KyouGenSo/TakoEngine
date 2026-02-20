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

  // インスタンスの取得
  static FrameTimer* GetInstance();
  // 初期化
  void Initialize();
  // 終了処理
  void Finalize();
  // 更新
  void Update();

  //----------------------------Getter----------------------------//
  // deltaTime の取得
  float GetDeltaTime() const { return deltaTime_; }
  // 実際の FPS の取得
  float GetFPS() const { return fps_; }
  // 表示用 FPS の取得
  float GetDisplayFPS() const { return displayFPS_; }
  // ゲーム起動からの経過時間の取得
  float GetGameTime() const { return gameTime_; }

private:
  // deltaTime と FPS の更新
  void UpdateDeltaTimeAndFPS();

  // gameTime の更新
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

  // 表示用 FPS
  float displayFPS_;

  // ゲーム起動からの経過時間
  float gameTime_;

  // 1秒間の計測用
  float timeAccumulator_;
  int frameCount_;
};

} // namespace Tako