#pragma once
#include<fstream>
#include <string>
#include <unordered_map>
#include <array>
#include <wrl.h>
#include "xaudio2.h"
#pragma comment(lib, "xaudio2.lib")

class Audio
{

private: // シングルトン設定

	// インスタンス
	static Audio* instance_;

	Audio() = default;
	~Audio() = default;
	Audio(Audio&) = delete;
	Audio& operator=(Audio&) = delete;

public: // 構造体
	struct ChunkHeader {
		char id[4]; // チャンクのID
		uint32_t size; // チャンクのサイズ
	};

	struct RiffHeader {
		ChunkHeader chunk; // "RIFF"
		char type[4]; // "WAVE"
	};

	struct FormatChunk {
		ChunkHeader chunk; // "fmt "
		WAVEFORMATEX fmt; // 波形フォーマット
	};

	struct SoundData {
		// 波形フォーマット
		WAVEFORMATEX wfex;
		// バッファの先頭アドレス
		BYTE* pBuffer;
		// バッファのサイズ
		unsigned int bufferSize;
	};

public:

	// インスタンスの取得
	static Audio* GetInstance();

	// サウンドの最大数
	static const int kMaxSoundNum = 3000;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const std::string& directoryPath);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// サウンドの読み込み
	/// </summary>
	uint32_t LoadWaveFile(const std::string& filename);

	/// <summary>
	/// サウンドの解放
	/// </summary>
	void SoundUnload(SoundData* soundData);

	/// <summary>
	/// サウンドの再生
	/// </summary>
	uint32_t PlayWave(uint32_t soundDataHandle, bool loopFlag, float volume);
	uint32_t PlayWave(uint32_t soundDataHandle);
	uint32_t PlayWave(uint32_t soundDataHandle, bool loopFlag);
	uint32_t PlayWave(uint32_t soundDataHandle, float volume);

	/// <summary>
	/// サウンド停止
	/// </summary>
	void StopWave(uint32_t voiceHandle);

	/// <summary>
	/// サウンド再生中かどうか
	/// </summary>
	bool IsPlaying(uint32_t voiceHandle);

	/// <summary>
	/// 音量の設定
	/// </summary>
	void SetVolume(uint32_t voiceHandle, float volume);

	/// <summary>
	/// ピッチの設定
	/// </summary>
	void SetPitch(uint32_t voiceHandle, float pitch);

private: // メンバー変数
	// XAudio2
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;

	// マスターボイス
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	std::array<std::string, kMaxSoundNum> soundNames_;

	// サウンドデータ
	std::array<SoundData, kMaxSoundNum> soundDatas_;
	//std::unordered_map<std::string, SoundData> soundDatas_;

	// ボイスデータ
	std::unordered_map<uint32_t, IXAudio2SourceVoice*> voiceDatas_;

	// ボイスの状態
	std::unordered_map<uint32_t, XAUDIO2_VOICE_STATE> voiceStates_;

	// サウンド格納ディレクトリ
	std::string directoryPath_;

	// 次に使うサウンドデータ番号
	uint32_t nextSoundIndex_ = 0u;
	// 次に使う再生中データ番号
	uint32_t nextVoiceHandle_ = 0u;

};