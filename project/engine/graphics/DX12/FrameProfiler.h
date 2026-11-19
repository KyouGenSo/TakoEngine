#pragma once
#include <chrono>
#include <cstdint>
#include <d3d12.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <wrl.h>

namespace Tako {

  class CommandContext;

  /// <summary>
  /// 計測した区間 1 つ（計測期間の、1 フレームあたりの平均）
  /// </summary>
  struct ProfileSample {
    std::string label;
    float       drawCalls  = 0.0f;
    float       cpuMs      = 0.0f;   ///< 区間の中でコマンドを積むのに掛かった時間
    float       gpuMs      = 0.0f;   ///< GPU が区間のコマンドを実行した時間（hasGpuTime が false なら 0）
    uint8_t     depth      = 0;      ///< 入れ子の深さ（0 が一番外）
    bool        isEvent    = false;  ///< パスを差し込める位置
    bool        hasGpuTime = false;
  };

  /// <summary>
  /// 1 フレームの時間の内訳（ms。計測期間の平均）
  /// </summary>
  struct FrameTimes {
    float updateMs   = 0.0f;  ///< CPU：更新
    float recordMs   = 0.0f;  ///< CPU：描画コマンドを積んで GPU へ渡すまで
    float gpuMs      = 0.0f;  ///< GPU：1 フレームのコマンド全部の実行
    float otherGpuMs = 0.0f;  ///< gpuMs のうち、どの区間にも入っていない分
    float gpuWaitMs  = 0.0f;  ///< CPU：GPU の完了待ち
    float idleMs     = 0.0f;  ///< CPU：FPS 上限の待ちと Present
  };

  /// <summary>
  /// フレームを区間に分けて、CPU がコマンドを積む時間と、GPU がそれを実行する時間（タイムスタンプ）を測る。区間は ProfileScope で囲む。
  /// 結果は計測期間（既定 0.5 秒）ごとの平均で、1 フレームおきにしか通らない区間は、1 回分の半分の時間になる
  /// ponytail: GPU の結果は、フレームごとに GPU の完了を待つ（DX12Basic::EndDraw）前提でその場で読む。待たない作りにしたら、数フレーム遅れて読むリングが要る
  /// </summary>
  class TAKO_API FrameProfiler {
  public: //定数
    static constexpr uint32_t kInvalidScope = UINT32_MAX;

  private: //定数
    static constexpr uint32_t kNoQuery      = UINT32_MAX;
    static constexpr uint32_t kMaxGpuScopes = 255;                      ///< 1 フレームにタイムスタンプを取れる区間の数（超えた区間は GPU 時間なし）
    static constexpr uint32_t kQueryCount   = (kMaxGpuScopes + 1) * 2;  ///< 区間ごとに 2 個と、フレーム全体の 2 個

  private: //構造体
    using Clock = std::chrono::steady_clock;

    /// <summary>
    /// 今のフレームで開いた区間
    /// </summary>
    struct Scope {
      std::string       label;
      Clock::time_point cpuBegin;
      float             cpuMs     = 0.0f;
      uint32_t          drawBegin = 0;         ///< 開いたときの CommandContext::GetDrawCount
      uint32_t          drawCalls = 0;
      uint32_t          query     = kNoQuery;  ///< 開始のタイムスタンプの番号（終了は +1）
      uint8_t           depth     = 0;
      bool              isEvent   = false;
    };

    /// <summary>
    /// 計測期間の合計（区間の並びを、フレームをまたいで保つ）
    /// </summary>
    struct Row {
      std::string label;
      float       cpuMs      = 0.0f;
      float       gpuMs      = 0.0f;
      uint32_t    drawCalls  = 0;
      uint32_t    frames     = 0;      ///< この期間に通ったフレームの数（0 のまま期間を終えたら行を消す）
      uint8_t     depth      = 0;
      bool        isEvent    = false;
      bool        hasGpuTime = false;
    };

  public: //メンバー関数
    /// <summary>
    /// タイムスタンプ用のヒープと読み戻し用のバッファを作る。作れない環境では、GPU 時間なしで測る
    /// </summary>
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, CommandContext* context);

    /// <summary>
    /// コマンドリストが記録できる状態になった直後に呼ぶ。SetEnabled の値はここで反映する
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// 区間を開く。開いている区間の中で開くと、その子になる
    /// </summary>
    /// <param name="isEvent">子を字下げするための入れ物（タイムスタンプを取らない）</param>
    /// <returns>EndScope に渡す番号（無効なら kInvalidScope）</returns>
    uint32_t BeginScope(std::string_view label, bool isEvent = false);

    void EndScope(uint32_t scope);

    /// <summary>
    /// このフレームのタイムスタンプを読み戻し用のバッファへ書くコマンドを積む。コマンドリストを閉じる前に呼ぶ
    /// </summary>
    void Resolve();

    /// <summary>
    /// このフレームの結果を期間の合計に足し、期間が過ぎていれば GetSamples / GetFrameTimes の中身を差し替える。GPU の完了を待った後に呼ぶ
    /// </summary>
    void ReadBack();

    /// <summary>
    /// 1 フレームの CPU 時間を足す（drawMs は、GPU の完了待ちと Present を含む描画全体）
    /// </summary>
    void AddCpuTimes(float updateMs, float drawMs);

    /// <summary>
    /// 描画の終わりに CPU が待った時間を足す
    /// </summary>
    void AddWaitTimes(float gpuWaitMs, float idleMs);

    //==========================================================================
    //Setter
    //==========================================================================
    /// <summary>
    /// 次の BeginFrame から計測する・しないを切り替える
    /// </summary>
    void SetEnabled(bool enabled) { isEnabledRequested_ = enabled; }

    /// <summary>
    /// 計測期間。seconds が過ぎ、かつ minFrames フレーム以上たったら結果を差し替える
    /// </summary>
    void SetWindow(float seconds, uint32_t minFrames = 1);

    //==========================================================================
    //Getter
    //==========================================================================
    bool IsEnabled() const { return isEnabled_; }
    std::span<const ProfileSample> GetSamples() const { return samples_; }
    const FrameTimes& GetFrameTimes() const { return frameTimes_; }

    /// <summary>
    /// 結果を差し替えた回数（表示を作り直すかどうかの判定用）
    /// </summary>
    uint32_t GetPublishCount() const { return publishCount_; }

  private: //非公開関数
    /// <summary>
    /// 期間の合計を 1 フレームあたりの平均にして公開し、次の期間を始める
    /// </summary>
    void Publish(Clock::time_point now);

    void ResetWindow(Clock::time_point now);

  private: //メンバー変数
    //GPU のタイムスタンプ
    CommandContext*                         context_     = nullptr;
    ID3D12GraphicsCommandList*              commandList_ = nullptr;  ///< context_ のコマンドリスト（バリアを発行させずに積むため、直接持つ）
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource>  readback_;
    uint64_t                                frequency_   = 0;        ///< タイムスタンプの 1 秒あたりの目盛り
    uint32_t                                queryCount_  = 0;        ///< このフレームで使った番号の数（0・1 はフレーム全体）

    //今のフレーム
#ifdef TAKO_EDITOR
    bool               isEnabledRequested_ = true;
#else
    bool               isEnabledRequested_ = false;
#endif
    bool               isEnabled_          = false;
    std::vector<Scope> scopes_;                      ///< 文字列の確保を使い回すため、縮めない
    uint32_t           scopeCount_         = 0;
    uint8_t            depth_              = 0;

    //計測期間
    std::vector<Row>  rows_;
    Clock::time_point windowStart_;
    Clock::duration   windowLength_    = std::chrono::milliseconds(500);
    uint32_t          minWindowFrames_ = 1;
    uint32_t          windowFrames_    = 0;
    uint32_t          cpuFrames_       = 0;
    uint32_t          waitFrames_      = 0;
    float             gpuSum_          = 0.0f;
    float             updateSum_       = 0.0f;
    float             drawSum_         = 0.0f;
    float             gpuWaitSum_      = 0.0f;
    float             idleSum_         = 0.0f;

    //公開する結果
    std::vector<ProfileSample> samples_;
    FrameTimes                 frameTimes_;
    uint32_t                   publishCount_ = 0;
  };

  /// <summary>
  /// 生きている間を 1 つの区間にする
  /// </summary>
  class ProfileScope {
  public: //メンバー関数
    ProfileScope(FrameProfiler& profiler, std::string_view label, bool isEvent = false) : profiler_(profiler), scope_(profiler.BeginScope(label, isEvent)) {}
    ~ProfileScope() { profiler_.EndScope(scope_); }
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

  private: //メンバー変数
    FrameProfiler& profiler_;
    uint32_t       scope_;
  };

#ifdef _DEBUG
  /// <summary>
  /// 目盛りから ms への変換と、区間の並び・入れ子・消える行を assert で確かめる。Debug の起動時に 1 回呼ぶ
  /// </summary>
  TAKO_API void RunFrameProfilerSelfTest();
#endif

} // namespace Tako
