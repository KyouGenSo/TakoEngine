#include "AnimationPlayer.h"
#include "FrameTimer.h"

#include <algorithm>
#include <utility>

namespace Tako {

  namespace {

    float Clock(bool unscaled) {
      const FrameTimer* timer = FrameTimer::GetInstance();
      return unscaled ? timer->GetGameTime() : timer->GetScaledTime();
    }

  } // namespace

  void AnimationPlayer::Play(const std::string& clipName, const std::string& target, const PlayOptions& options) {
    const AnimationClip* clip = AnimationClip::Find(clipName);
    if (!clip) {
      return;
    }
    auto it = FindPlayback(clipName, target);
    if (it == playbacks_.end()) {
      it = playbacks_.insert(playbacks_.end(), Playback{});
    }
    *it = Playback{ .clip = clip, .target = target, .options = options, .startClock = Clock(options.unscaledTime) };
  }

  void AnimationPlayer::Stop(const std::string& clipName, const std::string& target) {
    if (auto it = FindPlayback(clipName, target); it != playbacks_.end()) {
      playbacks_.erase(it);
    }
  }

  void AnimationPlayer::StopAll() {
    playbacks_.clear();
  }

  void AnimationPlayer::Update() {
    // コールバックの中で Play / Stop されても走査中の配列を壊さないよう、呼ぶのは最後にまとめる
    std::vector<std::pair<std::function<void(const std::string&)>, std::string>> fired;
    std::vector<const AnimEvent*>                                                passed;

    for (Playback& playback : playbacks_) {
      if (playback.isFinished) {
        continue;
      }
      const AnimationClip& clip    = *playback.clip;
      const float          elapsed = (Clock(playback.options.unscaledTime) - playback.startClock - playback.options.delay) * playback.options.speed;

      if (playback.options.onEvent) {
        passed.clear();
        clip.CollectEvents(playback.elapsed, elapsed, passed);
        for (const AnimEvent* event : passed) {
          fired.emplace_back(playback.options.onEvent, event->name);
        }
      }

      playback.elapsed    = elapsed;
      playback.time       = clip.WrapTime(elapsed);
      playback.isFinished = clip.GetLoopMode() == AnimationClip::LoopMode::Once && elapsed >= clip.GetDuration();
    }

    for (auto& [callback, name] : fired) {
      callback(name);
    }
  }

  AnimationPose AnimationPlayer::Evaluate(const std::string& name) const {
    AnimationPose pose;
#ifdef _DEBUG
    if (previewClip_) {
      previewClip_->EvaluateInto(previewTime_, name, previewTarget_, pose);
      return pose;
    }
#endif
    for (const Playback& playback : playbacks_) {
      playback.clip->EvaluateInto(playback.time, name, playback.target, pose);
    }
    return pose;
  }

#ifdef _DEBUG
  void AnimationPlayer::SetPreview(const AnimationClip* clip, const std::string& target, float time) {
    previewClip_   = clip;
    previewTarget_ = target;
    previewTime_   = time;
  }
#endif

  bool AnimationPlayer::IsPlaying(const std::string& clipName, const std::string& target) const {
    return std::ranges::any_of(playbacks_, [&](const Playback& playback) {
      return !playback.isFinished && playback.target == target && playback.clip->GetName() == clipName;
    });
  }

  std::vector<AnimationPlayer::Playback>::iterator AnimationPlayer::FindPlayback(const std::string& clipName, const std::string& target) {
    return std::ranges::find_if(playbacks_, [&](const Playback& playback) {
      return playback.target == target && playback.clip->GetName() == clipName;
    });
  }

} // namespace Tako
