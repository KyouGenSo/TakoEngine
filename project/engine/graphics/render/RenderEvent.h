#pragma once
#include <cstdint>
#include <iterator>

namespace Tako {

  /// <summary>
  /// 本編の描画で追加のパスやポストエフェクトを差し込める位置（Unity の RenderPassEvent に相当）
  /// </summary>
  enum class RenderEvent : uint8_t {
    BeforeShadow,
    AfterOpaque,        ///< 不透明の直後（デカールの前）
    BeforeTransparent,  ///< デカールの後、半透明の前
    AfterTransparent,   ///< 半透明・パーティクル・線の後
    BeforePostProcess,
    AfterPostProcess,   ///< 描画先は最終結果の RT（UI の前）
    BeforeUI,
    AfterUI,
    Count
  };

  // 保存ファイルと編集 UI で使う名前。並びは列挙の値と一致させる
  inline constexpr const char* kRenderEventNames[] = { "BeforeShadow", "AfterOpaque", "BeforeTransparent", "AfterTransparent", "BeforePostProcess", "AfterPostProcess", "BeforeUI", "AfterUI" };
  static_assert(std::size(kRenderEventNames) == static_cast<size_t>(RenderEvent::Count));

  /// <summary>
  /// シーン（リニアの HDR）を画面の色へ直すときの、明るい所の縮め方
  /// </summary>
  enum class ToneMapping : uint32_t {
    None,  ///< 1 を超える所は 1 で切る（1 以下は sRGB へ直すだけ）
    ACES,  ///< 映画の規格 ACES の近似式（Narkowicz 2015）。明るい所をなだらかに縮め、暗い所の黒をやや締める
    Count
  };

  inline constexpr const char* kToneMappingNames[] = { "None", "ACES" };
  static_assert(std::size(kToneMappingNames) == static_cast<size_t>(ToneMapping::Count));

  /// <summary>
  /// ポストエフェクトの最後（ToneMapToDisplay）で掛ける値
  /// </summary>
  struct ToneMapSettings {
    ToneMapping mode     = ToneMapping::None;
    float       exposure = 0.0f;               ///< 露出（EV）。1 上げると 2 倍の明るさ。トーンマップの前に掛ける
  };

} // namespace Tako
