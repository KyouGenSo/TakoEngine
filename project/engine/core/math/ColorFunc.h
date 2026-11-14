#pragma once
#include "Vector4.h"

#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace Tako {

  struct ColorHSV;
  struct Color32;

  /// <summary>
  /// RGBA の色。各成分は 0〜1（1 を超える値も持てる）。Vector4 とは互いに自動で変わる
  /// </summary>
  struct Color final {
    float r;  ///< 赤
    float g;  ///< 緑
    float b;  ///< 青
    float a;  ///< 不透明度

    static const Color kWhite;
    static const Color kBlack;
    static const Color kClear;    ///< 透明（RGBA すべて 0）
    static const Color kRed;
    static const Color kGreen;
    static const Color kBlue;
    static const Color kYellow;
    static const Color kCyan;
    static const Color kMagenta;
    static const Color kGray;     ///< RGB すべて 0.5

    Color() = default;
    constexpr Color(float red, float green, float blue, float alpha = 1.0f) : r(red), g(green), b(blue), a(alpha) {}
    constexpr Color(const Vector4& v) : r(v.x), g(v.y), b(v.z), a(v.w) {}
    constexpr operator Vector4() const { return { r, g, b, a }; }

    /// <summary>
    /// 0xRRGGBB 形式の 16 進数から色を作る
    /// </summary>
    static constexpr Color FromHex(uint32_t rgb, float alpha = 1.0f)
    {
      return { static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f, static_cast<float>(rgb & 0xFF) / 255.0f, alpha };
    }

    /// <summary>
    /// HSV から色を作る。hue は 0〜1 で 1 周し範囲外は巡回する。saturation / value は 0〜1
    /// </summary>
    TAKO_API static Color FromHSV(float hue, float saturation, float value, float alpha = 1.0f);

    /// <summary>
    /// 色温度（ケルビン）から色を作る。1000〜40000 に丸める。6600 で白、低いほど赤く、高いほど青い
    /// </summary>
    TAKO_API static Color FromKelvin(float kelvin);

    /// <summary>
    /// 成分ごとに線形補間する（t は 0〜1 に丸めない）
    /// </summary>
    static constexpr Color Lerp(const Color& from, const Color& to, float t)
    {
      return { from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t, from.b + (to.b - from.b) * t, from.a + (to.a - from.a) * t };
    }

    /// <summary>
    /// HSV で補間する。色相は色相環の近い方を回る（緑から赤の途中が黄になる）
    /// </summary>
    TAKO_API static Color LerpHSV(const Color& from, const Color& to, float t);

    /// <summary>
    /// アルファだけを差し替えた色
    /// </summary>
    constexpr Color WithAlpha(float alpha) const { return { r, g, b, alpha }; }

    TAKO_API ColorHSV ToHSV() const;

    /// <summary>
    /// 0〜255 の色。各成分を 0〜1 に丸めてから変換する
    /// </summary>
    constexpr Color32 ToColor32() const;

    // Vector4 と混ぜた式で候補が 2 つにならないよう、演算子はメンバーにする。== は Vector4 のものを使う
    constexpr Color operator+(const Color& c) const { return { r + c.r, g + c.g, b + c.b, a + c.a }; }
    constexpr Color operator-(const Color& c) const { return { r - c.r, g - c.g, b - c.b, a - c.a }; }

    /// <summary>
    /// 成分ごとの掛け算（色で染める）
    /// </summary>
    constexpr Color operator*(const Color& c) const { return { r * c.r, g * c.g, b * c.b, a * c.a }; }

    /// <summary>
    /// アルファも含めて s 倍する
    /// </summary>
    constexpr Color operator*(float s) const { return { r * s, g * s, b * s, a * s }; }
    constexpr Color operator/(float s) const { return { r / s, g / s, b / s, a / s }; }
    friend constexpr Color operator*(float s, const Color& c) { return c * s; }
  };

  /// <summary>
  /// HSV の色。h（色相）は 0〜1 で 1 周、s（彩度）と v（明度）は 0〜1
  /// </summary>
  struct ColorHSV final {
    float h;
    float s;
    float v;
    float a;  ///< 不透明度

    ColorHSV() = default;
    constexpr ColorHSV(float hue, float saturation, float value, float alpha = 1.0f) : h(hue), s(saturation), v(value), a(alpha) {}

    /// <summary>
    /// RGBA の色にする。h が 0〜1 の外なら巡回する
    /// </summary>
    TAKO_API Color ToColor() const;
  };

  /// <summary>
  /// 各成分を 0〜255 の整数で持つ色。Color には自動で変わる
  /// </summary>
  struct Color32 final {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    Color32() = default;
    constexpr Color32(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) : r(red), g(green), b(blue), a(alpha) {}

    /// <summary>
    /// 0xRRGGBB 形式の 16 進数から作る
    /// </summary>
    static constexpr Color32 FromHex(uint32_t rgb, uint8_t alpha = 255)
    {
      return { static_cast<uint8_t>((rgb >> 16) & 0xFF), static_cast<uint8_t>((rgb >> 8) & 0xFF), static_cast<uint8_t>(rgb & 0xFF), alpha };
    }

    /// <summary>
    /// 0xRRGGBB 形式の 16 進数。アルファは入らない
    /// </summary>
    constexpr uint32_t ToHex() const { return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b); }

    constexpr operator Color() const { return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f }; }
  };

  constexpr Color32 Color::ToColor32() const
  {
    constexpr auto toByte = [](float value) { return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return { toByte(r), toByte(g), toByte(b), toByte(a) };
  }

  inline constexpr Color Color::kWhite   = { 1.0f, 1.0f, 1.0f, 1.0f };
  inline constexpr Color Color::kBlack   = { 0.0f, 0.0f, 0.0f, 1.0f };
  inline constexpr Color Color::kClear   = { 0.0f, 0.0f, 0.0f, 0.0f };
  inline constexpr Color Color::kRed     = { 1.0f, 0.0f, 0.0f, 1.0f };
  inline constexpr Color Color::kGreen   = { 0.0f, 1.0f, 0.0f, 1.0f };
  inline constexpr Color Color::kBlue    = { 0.0f, 0.0f, 1.0f, 1.0f };
  inline constexpr Color Color::kYellow  = { 1.0f, 1.0f, 0.0f, 1.0f };
  inline constexpr Color Color::kCyan    = { 0.0f, 1.0f, 1.0f, 1.0f };
  inline constexpr Color Color::kMagenta = { 1.0f, 0.0f, 1.0f, 1.0f };
  inline constexpr Color Color::kGray    = { 0.5f, 0.5f, 0.5f, 1.0f };

  static_assert(sizeof(Color) == sizeof(Vector4) && std::is_trivially_copyable_v<Color>, "GPU に送る構造体で Vector4 の代わりに置けるよう、同じ大きさのままにする");

#ifdef _DEBUG
  /// <summary>
  /// 色の変換と Gradient を assert で確かめる。Debug の起動時に 1 回呼ぶ
  /// </summary>
  TAKO_API void RunColorSelfTest();
#endif

} // namespace Tako
