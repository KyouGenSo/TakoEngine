#pragma once
#include "AnimationClip.h"
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace Tako {

  /// <summary>
  /// 複数のクリップを同時に再生し、要素名ごとの上乗せの値を合成する
  /// </summary>
  class AnimationPlayer {
  public: //構造体
    struct PlayOptions {
      float                                   delay        = 0.0f;   ///< 秒。待っている間は 0 秒の姿勢を表示する
      float                                   speed        = 1.0f;
      bool                                    unscaledTime = false;  ///< true なら時間スケールの影響を受けない
      std::function<void(const std::string&)> onEvent;               ///< イベントの時刻を過ぎたとき、その名前で呼ぶ
    };

  private: //構造体
    struct Playback {
      const AnimationClip* clip       = nullptr;
      std::string          target;
      PlayOptions          options;
      float                startClock = 0.0f;
      float                elapsed    = std::numeric_limits<float>::lowest();  ///< 遅延後からの経過秒（遅延中は負）。初期値は 0 秒のイベントも拾うため
      float                time       = 0.0f;                                  ///< クリップ内の時刻
      bool                 isFinished = false;
    };

  public: //メンバー関数
    /// <summary>
    /// clipName を target に対して最初から再生する。同じ組み合わせがあれば最初からやり直す
    /// </summary>
    /// <param name="clipName">AnimationClip::Find で引く名前。見つからなければ何もしない</param>
    /// <param name="target">target が空のトラックを適用する要素名（Object3d では空のまま）</param>
    /// <param name="options">遅延・速度・イベントの受け取り</param>
    void Play(const std::string& clipName, const std::string& target = "", const PlayOptions& options = {});

    /// <summary>
    /// 止めて上乗せを外す
    /// </summary>
    void Stop(const std::string& clipName, const std::string& target = "");

    void StopAll();

    /// <summary>
    /// 現在時刻を求め、通過したイベントを呼ぶ。同じフレームに何度呼んでも結果は変わらない
    /// </summary>
    void Update();

    /// <summary>
    /// name を対象にした全ての再生を重ねた値
    /// </summary>
    [[nodiscard]] AnimationPose Evaluate(const std::string& name) const;

#ifdef _DEBUG
    /// <summary>
    /// ゲーム側の再生の代わりに clip の time の姿勢を使う（エディタのプレビュー用）。nullptr で戻す
    /// </summary>
    void SetPreview(const AnimationClip* clip, const std::string& target, float time);
#endif

    //============================================================
    //Getter
    //============================================================
    /// <summary>
    /// 再生中か（遅延中を含む。Once が最後まで進んだら false だが、Stop までは最後の姿勢を保つ）
    /// </summary>
    [[nodiscard]] bool IsPlaying(const std::string& clipName, const std::string& target = "") const;

  private: //非公開関数
    std::vector<Playback>::iterator FindPlayback(const std::string& clipName, const std::string& target);

  private: //メンバー変数
    std::vector<Playback> playbacks_;

#ifdef _DEBUG
    //エディタのプレビュー
    const AnimationClip* previewClip_   = nullptr;
    std::string          previewTarget_;
    float                previewTime_   = 0.0f;
#endif
  };

} // namespace Tako
