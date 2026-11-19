#pragma once
#include <cstdint>
#include <memory>

namespace Tako {

  class FrameProfiler;
  class Text;

  /// <summary>
  /// FrameProfiler の結果を画面の左上に、FPS を右上に、文字で重ねて出す（エディタの無い構成でも見られる）
  /// </summary>
  class TAKO_API ProfileOverlay {
  public: //メンバー関数
    ProfileOverlay();
    ~ProfileOverlay();

    /// <summary>
    /// 結果が差し替わっていれば文字を作り直す。頂点バッファを作り直すので、描画の前の更新で呼ぶ
    /// </summary>
    void Update(const FrameProfiler& profiler);

    /// <summary>
    /// 描画リストへ登録する
    /// </summary>
    void Draw();

  private: //メンバー変数
    std::unique_ptr<Text> names_;
    std::unique_ptr<Text> cpuValues_;                  ///< 右寄せ
    std::unique_ptr<Text> gpuValues_;                  ///< 右寄せ
    std::unique_ptr<Text> fps_;                        ///< 画面の右上
    uint32_t              publishCount_ = UINT32_MAX;  ///< 文字を作ったときの FrameProfiler::GetPublishCount
    float                 shownFps_     = -1.0f;       ///< 文字を作ったときの FPS
  };

} // namespace Tako
