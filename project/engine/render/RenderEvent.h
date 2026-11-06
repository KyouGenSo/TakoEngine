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

} // namespace Tako
