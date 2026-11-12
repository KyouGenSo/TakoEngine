#include "AudioSourceComponent.h"
#include "Actor.h"
#include "Audio.h"

namespace Tako {

  void AudioSourceComponent::BeginPlay()
  {
    if (playOnStart_ && GetOwner()->IsActiveInHierarchy()) {
      Play();
    }
  }

  void AudioSourceComponent::OnActiveChanged(bool active)
  {
    if (!active) {
      Stop();
    } else if (playOnStart_ && GetOwner()->HasBegunPlay()) {
      Play();
    }
  }

  void AudioSourceComponent::EndPlay()
  {
    Stop();
  }

  void AudioSourceComponent::PostEditChange()
  {
    Audio* audio = Audio::GetInstance();
    if (voice_ && audio->IsPlaying(*voice_)) {
      audio->SetVolume(*voice_, volume_);
    }
  }

  void AudioSourceComponent::Play()
  {
    Stop();
    if (sound_.empty()) {
      return;
    }
    Audio*                        audio = Audio::GetInstance();
    const std::optional<uint32_t> data  = sound_.ends_with(".mp3") ? audio->LoadMP3File(sound_) : audio->LoadWaveFile(sound_);
    if (data) {
      voice_ = audio->Play(*data, loop_, volume_);
    }
  }

  void AudioSourceComponent::Stop()
  {
    if (voice_) {
      Audio::GetInstance()->StopWave(*voice_);
      voice_.reset();
    }
  }

} // namespace Tako
