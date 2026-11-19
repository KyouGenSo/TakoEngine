#pragma once
#include <cstdint>
#include <memory>

namespace Tako {

  class FrameProfiler;
  class Text;

  /// <summary>
  /// FrameProfiler の結果を画面の左上に、FPS を右上に、文字で重ねて出す（エディタの無い構成でも見られる）。
  /// 結果は 2 つの表を横に並べる（左：1 フレームの内訳と更新の区間、右：描画の区間）
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
    std::unique_ptr<Text> frameNames_;                    ///< 左の表の名前
    std::unique_ptr<Text> frameValues_;                   ///< 右寄せ
    std::unique_ptr<Text> renderNames_;                   ///< 右の表の名前
    std::unique_ptr<Text> renderCpuValues_;               ///< 右寄せ
    std::unique_ptr<Text> renderGpuValues_;               ///< 右寄せ
    std::unique_ptr<Text> fps_;                           ///< 画面の右上
    uint32_t              publishCount_    = UINT32_MAX;  ///< 文字を作ったときの FrameProfiler::GetPublishCount
    float                 shownFps_        = -1.0f;       ///< 文字を作ったときの FPS
  };

} // namespace Tako
