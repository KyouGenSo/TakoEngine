#pragma once
#include<fstream>
#include <string>
#include <unordered_map>
#include <array>
#include <memory>
#include <wrl.h>
#include "xaudio2.h"
#pragma comment(lib, "xaudio2.lib")

namespace Tako {

/// <summary>
/// オーディオ管理クラス。XAudio2を使用して WAVE/MP3ファイルの読み込み、再生、音量/ピッチ制御を行う
/// </summary>
class Audio
{

private: // シングルトン設定
	static std::unique_ptr<Audio> instance_;

	Audio() = default;
	~Audio() = default;

	friend struct std::default_delete<Audio>;

public:
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

public: //定数
	// サウンドの最大数
	static const int kMaxSoundNum = 3000;

public: //構造体
	struct ChunkHeader {
		char     id[4];  ///< チャンクの ID
		uint32_t size;   ///< チャンクのサイズ
	};

	struct RiffHeader {
		ChunkHeader chunk;    ///< "RIFF"
		char        type[4];  ///< "WAVE"
	};

	struct FormatChunk {
		ChunkHeader  chunk;  ///< "fmt "
		WAVEFORMATEX fmt;    ///< 波形フォーマット
	};

	struct SoundData {
		WAVEFORMATEX            wfex;        ///< 波形フォーマット
		std::unique_ptr<BYTE[]> pBuffer;     ///< バッファの先頭アドレス
		unsigned int            bufferSize;  ///< バッファのサイズ
	};

public: //メンバー関数

	/// <summary>
	/// シングルトンインスタンスを取得
	/// </summary>
	/// <returns>Audio のインスタンス</returns>
	static Audio* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="directoryPath">サウンドファイルの格納ディレクトリパス</param>
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
	/// WAVE ファイルの読み込み
	/// </summary>
	/// <param name="filename">ファイル名（directoryPath 基準の相対パス）</param>
	/// <returns>サウンドデータハンドル</returns>
	uint32_t LoadWaveFile(const std::string& filename);

	/// <summary>
	/// MP3ファイルの読み込み（miniaudio 使用）
	/// </summary>
	/// <param name="filename">ファイル名（directoryPath 基準の相対パス）</param>
	/// <returns>サウンドデータハンドル</returns>
	uint32_t LoadMP3File(const std::string& filename);

	/// <summary>
	/// サウンドデータの解放
	/// </summary>
	/// <param name="soundData">解放するサウンドデータ</param>
	void SoundUnload(SoundData* soundData);

	/// <summary>
	/// サウンドの再生（全パラメータ指定）
	/// </summary>
	/// <param name="soundDataHandle">サウンドデータハンドル</param>
	/// <param name="loopFlag">ループ再生する場合 true</param>
	/// <param name="volume">音量（0.0 ~ 1.0）</param>
	/// <returns>ボイスハンドル</returns>
	uint32_t Play(uint32_t soundDataHandle, bool loopFlag, float volume);

	/// <summary>
	/// サウンドの再生（デフォルト設定）
	/// </summary>
	/// <param name="soundDataHandle">サウンドデータハンドル</param>
	/// <returns>ボイスハンドル</returns>
	uint32_t Play(uint32_t soundDataHandle);

	/// <summary>
	/// サウンドの再生（ループ指定）
	/// </summary>
	/// <param name="soundDataHandle">サウンドデータハンドル</param>
	/// <param name="loopFlag">ループ再生する場合 true</param>
	/// <returns>ボイスハンドル</returns>
	uint32_t Play(uint32_t soundDataHandle, bool loopFlag);

	/// <summary>
	/// サウンドの再生（音量指定）
	/// </summary>
	/// <param name="soundDataHandle">サウンドデータハンドル</param>
	/// <param name="volume">音量（0.0 ~ 1.0）</param>
	/// <returns>ボイスハンドル</returns>
	uint32_t Play(uint32_t soundDataHandle, float volume);

	/// <summary>
	/// サウンドの再生を停止
	/// </summary>
	/// <param name="voiceHandle">停止するボイスハンドル</param>
	void StopWave(uint32_t voiceHandle);

	//==============================================
	//Setter
	//==============================================
	/// <summary>
	/// 音量を設定
	/// </summary>
	/// <param name="voiceHandle">対象ボイスハンドル</param>
	/// <param name="volume">音量（0.0 ~ 1.0）</param>
	void SetVolume(uint32_t voiceHandle, float volume);

	/// <summary>
	/// ピッチ（再生速度）を設定
	/// </summary>
	/// <param name="voiceHandle">対象ボイスハンドル</param>
	/// <param name="pitch">ピッチ倍率（1.0が標準、範囲: 0.5 ~ 2.0）</param>
	void SetPitch(uint32_t voiceHandle, float pitch);

	//==============================================
	//Getter
	//==============================================
	/// <summary>
	/// サウンドが再生中かどうかを判定
	/// </summary>
	/// <param name="voiceHandle">判定するボイスハンドル</param>
	/// <returns>再生中の場合 true</returns>
	bool IsPlaying(uint32_t voiceHandle);

private: //メンバー変数
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;  ///< XAudio2

    IXAudio2MasteringVoice* masterVoice_ = nullptr;  ///< マスターボイス

    std::array<std::string, kMaxSoundNum> soundNames_;  ///< 読み込み済みファイル名。重複読み込みの判定に使用（添字 = サウンドハンドル）

    std::array<SoundData, kMaxSoundNum> soundDatas_;  ///< 添字 = サウンドハンドル

    std::unordered_map<uint32_t, IXAudio2SourceVoice*> voiceDatas_;  ///< ボイスハンドル -> 再生中ソースボイス

    std::unordered_map<uint32_t, XAUDIO2_VOICE_STATE> voiceStates_;  ///< 未使用

    std::string directoryPath_;

    uint32_t nextSoundIndex_  = 0u;  ///< 次に割り当てるサウンドデータ番号（= 返すサウンドハンドル）
    uint32_t nextVoiceHandle_ = 0u;  ///< 次に割り当てるボイスハンドル。Play ごとに加算し再利用しない

};

} // namespace Tako