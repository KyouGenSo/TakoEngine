#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "Audio.h"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <cstring>

namespace Tako {

  std::unique_ptr<Audio> Audio::instance_ = nullptr;

  Audio* Audio::GetInstance()
  {
    if (!instance_) {
      instance_ = std::make_unique<Audio>(Token{});
    }

    return instance_.get();
  }

  void Audio::Initialize(const std::string& directoryPath)
  {
    HRESULT hr;

    directoryPath_ = directoryPath;

    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);

    hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
  }

  void Audio::Finalize()
  {
    // マスターボイスの解放
    if (masterVoice_ != nullptr) {
      masterVoice_->DestroyVoice();
      masterVoice_ = nullptr;
    }

    // サウンドデータの解放
    for (auto& soundData : soundDatas_) {
      SoundUnload(&soundData);
    }

    // voiceDatas_の解放
    for (auto& voiceData : voiceDatas_) {
      if (voiceData.second != nullptr) {
        voiceData.second->DestroyVoice();
        voiceData.second = nullptr;
      }
    }

    // インスタンスの解放
    instance_.reset();
  }

  void Audio::Update()
  {
    // 再生終了したボイスの解放
    for (auto& voiceData : voiceDatas_) {
      if (voiceData.second != nullptr) {
        XAUDIO2_VOICE_STATE state;
        voiceData.second->GetState(&state);

        if (state.BuffersQueued == 0) {
          voiceData.second->DestroyVoice();
          voiceData.second = nullptr;

        }
      }
    }

    // 再生終了したボイスの削除
    std::erase_if(voiceDatas_, [](const auto& voiceData) { return voiceData.second == nullptr; });
  }

  uint32_t Audio::LoadWaveFile(const std::string& filename)
  {
    // ファイル名の重複チェック
    for (uint32_t i = 0; i < nextSoundIndex_; ++i) {
      if (soundNames_[i] == filename) {
        // すでに読み込まれている場合はそのハンドルを返す
        return i;
      }
    }

    // サウンドデータの取得
    SoundData& soundData = soundDatas_[nextSoundIndex_];

    // ファイルを開く
    std::ifstream file;
    // バイナリモードで開く
    file.open(directoryPath_ + filename, std::ios::binary);
    assert(file.is_open());

    RiffHeader riff;

    do {
      // wav ファイルのヘッダーを読み込む
      file.read(reinterpret_cast<char*>(&riff), sizeof(riff));

      if (strncmp(riff.chunk.id, "RIFF", 4) == 0 && strncmp(riff.type, "WAVE", 4) == 0) {
        break;  // 「RIFF WAVE」チャンクが見つかったので探索を停止する
      }

      // 「RIFF WAVE」チャンク以外をスキップする
      file.seekg(riff.chunk.size, std::ios::cur);

    } while (!file.eof());

    if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0) {
      assert(false);
    }

    FormatChunk format = {};

    do {
      file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader));

      if (strncmp(format.chunk.id, "fmt ", 4) == 0) {
        break;  // 「fmt 」チャンクが見つかったので、探索を停止する
      }

      // 「fmt 」チャンク以外のチャンクをスキップする
      file.seekg(format.chunk.size, std::ios::cur);

    } while (!file.eof());

    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
      assert(false);
    }
    assert(format.chunk.size <= sizeof(format.fmt));
    file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);


    ChunkHeader data;
    do {
      file.read(reinterpret_cast<char*>(&data), sizeof(data));

      if (strncmp(data.id, "data", 4) == 0) {
        break;  // 「data」チャンクが見つかったので、探索を停止する
      }

      // 「data」チャンク以外のチャンクをスキップする（例えば「JUNK」やその他のチャンク）
      file.seekg(data.size, std::ios::cur);

    } while (!file.eof());

    if (strncmp(data.id, "data", 4) != 0) {
      assert(false);
    }

    auto pBuffer = std::make_unique<char[]>(data.size);
    file.read(pBuffer.get(), data.size);

    file.close();

    soundData.wfex = format.fmt;
    soundData.pBuffer = std::unique_ptr<BYTE[]>(reinterpret_cast<BYTE*>(pBuffer.release()));
    soundData.bufferSize = data.size;

    // 名前の登録
    soundNames_[nextSoundIndex_] = filename;

    // ハンドルの返却
    uint32_t handle = nextSoundIndex_;
    nextSoundIndex_++;

    return handle;
  }

  uint32_t Audio::LoadMP3File(const std::string& filename)
  {
    // ファイル名の重複チェック
    for (uint32_t i = 0; i < nextSoundIndex_; ++i) {
      if (soundNames_[i] == filename) {
        // すでに読み込まれている場合はそのハンドルを返す
        return i;
      }
    }

    SoundData& soundData = soundDatas_[nextSoundIndex_];
    // フルパス（ディレクトリパスと連結）
    std::string fullPath = directoryPath_ + filename;
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0); // native チャンネル、サンプルレートで float 出力
    if (ma_decoder_init_file(fullPath.c_str(), &config, &decoder) != MA_SUCCESS) {
      return 0;
    }
    ma_uint64 totalFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS) {
      ma_decoder_uninit(&decoder);
      return 0;
    }
    // チャンネル数、サンプルレートは decoder から取得（ゼロの場合は config の値を使用）
    ma_uint32 channels = decoder.outputChannels;
    if (channels == 0) channels = config.channels;
    ma_uint32 sampleRate = decoder.outputSampleRate;
    if (sampleRate == 0) sampleRate = config.sampleRate;
    size_t bufferSize = static_cast<size_t>(totalFrames * channels * sizeof(float));
    auto pBuffer = std::make_unique<float[]>(totalFrames * channels);
    ma_uint64 framesRead = 0;
    ma_result result = ma_decoder_read_pcm_frames(&decoder, pBuffer.get(), totalFrames, &framesRead);
    (void)result; // 未使用の変数であることを明示

    // デコード完了後、decoder を解放
    ma_decoder_uninit(&decoder);

    // WAVEFORMATEX を float フォーマットに合わせて設定
    soundData.wfex.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    soundData.wfex.nChannels = static_cast<WORD>(channels);
    soundData.wfex.nSamplesPerSec = sampleRate;
    soundData.wfex.wBitsPerSample = 32;
    soundData.wfex.nBlockAlign = static_cast<WORD>(channels * 4);
    soundData.wfex.nAvgBytesPerSec = sampleRate * soundData.wfex.nBlockAlign;
    soundData.wfex.cbSize = 0;
    soundData.pBuffer = std::unique_ptr<BYTE[]>(reinterpret_cast<BYTE*>(pBuffer.release()));
    soundData.bufferSize = static_cast<unsigned int>(bufferSize);
    soundNames_[nextSoundIndex_] = filename;
    uint32_t handle = nextSoundIndex_;
    nextSoundIndex_++;
    return handle;
  }

  void Audio::SoundUnload(SoundData* soundData)
  {
    if (soundData->pBuffer) {
      soundData->pBuffer.reset();
      soundData->bufferSize = 0;
      soundData->wfex = {};
    }
  }

  uint32_t Audio::Play(uint32_t soundDataHandle, bool loopFlag, float volume)
  {
    HRESULT hr;

    // サウンドデータの取得
    SoundData& soundData = soundDatas_[soundDataHandle];

    // ボイスの作成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr));

    // ボイスの設定
    pSourceVoice->SetVolume(volume);

    // バッファの設定
    XAUDIO2_BUFFER buffer = {};
    buffer.pAudioData = soundData.pBuffer.get();
    buffer.AudioBytes = soundData.bufferSize;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loopFlag ? XAUDIO2_LOOP_INFINITE : 0;

    // ボイスの再生
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    hr = pSourceVoice->Start();

    // ハンドルの返却
    uint32_t handle = nextVoiceHandle_;
    nextVoiceHandle_++;

    // ボイスの登録
    voiceDatas_[handle] = pSourceVoice;

    return handle;
  }

  uint32_t Audio::Play(uint32_t soundDataHandle)
  {
    return Play(soundDataHandle, false, 1.0f);
  }

  uint32_t Audio::Play(uint32_t soundDataHandle, bool loopFlag)
  {
    return Play(soundDataHandle, loopFlag, 1.0f);
  }

  uint32_t Audio::Play(uint32_t soundDataHandle, float volume)
  {
    return Play(soundDataHandle, false, volume);
  }

  void Audio::StopWave(uint32_t voiceHandle)
  {
    if (voiceDatas_.find(voiceHandle) != voiceDatas_.end()) {
      voiceDatas_.at(voiceHandle)->Stop();
    }
    else {
      return;
    }

  }

  bool Audio::IsPlaying(uint32_t voiceHandle)
  {
    XAUDIO2_VOICE_STATE state;
    if (!voiceDatas_.contains(voiceHandle)) {
      return false;
    }

    voiceDatas_.at(voiceHandle)->GetState(&state);

    return state.BuffersQueued > 0;
  }

  void Audio::SetVolume(uint32_t voiceHandle, float volume)
  {
    voiceDatas_.at(voiceHandle)->SetVolume(volume);
  }

  void Audio::SetPitch(uint32_t voiceHandle, float pitch)
  {
    voiceDatas_.at(voiceHandle)->SetFrequencyRatio(pitch);
  }

} // namespace Tako
