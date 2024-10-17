#include "Audio.h"
#include <cassert>

Audio* Audio::instance_ = nullptr;

Audio* Audio::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new Audio();
	}

	return instance_;
}

void Audio::Initialize(const std::string& directoryPath)
{
	HRESULT hr;

	// ディレクトリパスの設定
	directoryPath_ = directoryPath;

	// XAudio2の初期化
	hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);

	// マスターボイスの作成
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
}

void Audio::Finalize()
{
	// マスターボイスの解放
	if (masterVoice_ != nullptr)
	{
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}

	// サウンドデータの解放
	for (auto& soundData : soundDatas_)
	{
		SoundUnload(&soundData);
	}

	// voiceDatas_の解放
	for (auto& voiceData : voiceDatas_)
	{
		if (voiceData.second != nullptr)
		{
			voiceData.second->DestroyVoice();
			voiceData.second = nullptr;
		}
	}

	// インスタンスの解放
	if (instance_ != nullptr)
	{
		delete instance_;
		instance_ = nullptr;
	}
}

void Audio::Update()
{
	// 再生終了したボイスの解放
	for (auto& voiceData : voiceDatas_)
	{
		if (voiceData.second != nullptr)
		{
			XAUDIO2_VOICE_STATE state;
			voiceData.second->GetState(&state);

			if (state.BuffersQueued == 0)
			{
				voiceData.second->DestroyVoice();
				voiceData.second = nullptr;
				
				//voiceDatas_.erase(voiceData.first);
			}
		}
	}
}

uint32_t Audio::LoadWaveFile(const char* filename)
{	
	// サウンドデータの取得
	SoundData& soundData = soundDatas_[nextSoundIndex_];

	// ファイルを開く
	std::ifstream file;
	// バイナリモードで開く
	file.open(directoryPath_ + filename, std::ios::binary);
	assert(file.is_open());

	// wavファイルのヘッダーを読み込む
	RiffHeader riff;
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));

	if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(false);
	}

	FormatChunk format = {};
	file.read(reinterpret_cast<char*>(&format), sizeof(ChunkHeader));

	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
	{
		assert(false);
	}
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);


	ChunkHeader data;
	file.read(reinterpret_cast<char*>(&data), sizeof(data));

	if (strncmp(data.id, "JUNK", 4) == 0)
	{
		file.seekg(data.size, std::ios::cur);
		file.read(reinterpret_cast<char*>(&data), sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0)
	{
		assert(false);
	}

	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	file.close();

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	// ハンドルの返却
	uint32_t handle = nextSoundIndex_;
	nextSoundIndex_++;

	return handle;
}

void Audio::SoundUnload(SoundData* soundData)
{
	if (soundData->pBuffer != nullptr)
	{
		delete[] soundData->pBuffer;
		soundData->pBuffer = 0;
		soundData->bufferSize = 0;
		soundData->wfex = {};
	}

	soundData = nullptr;
}

uint32_t Audio::PlayWave(uint32_t soundDataHandle, bool loopFlag, float volume)
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
	buffer.pAudioData = soundData.pBuffer;
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

uint32_t Audio::PlayWave(uint32_t soundDataHandle)
{
	return PlayWave(soundDataHandle, false, 1.0f);
}

uint32_t Audio::PlayWave(uint32_t soundDataHandle, bool loopFlag)
{
	return PlayWave(soundDataHandle, loopFlag, 1.0f);
}

uint32_t Audio::PlayWave(uint32_t soundDataHandle, float volume)
{
	return PlayWave(soundDataHandle, false, volume);
}

void Audio::StopWave(uint32_t voiceHandle)
{
	voiceDatas_.at(voiceHandle)->Stop();
}

bool Audio::IsPlaying(uint32_t voiceHandle)
{
	XAUDIO2_VOICE_STATE state;
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
