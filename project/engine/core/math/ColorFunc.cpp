#include "ColorFunc.h"

#include <cmath>

#ifdef _DEBUG
#include "Gradient.h"
#include "RandomEngine.h"

#include <cassert>
#endif

namespace Tako
{
  Color Color::FromHSV(float hue, float saturation, float value, float alpha) {
    return ColorHSV(hue, saturation, value, alpha).ToColor();
  }

  Color Color::FromKelvin(float kelvin) {
    // Tanner Helland の近似式。黒体の色の表に合わせた式で、100K 単位の温度から 0〜255 の値を出す
    const float temperature = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;
    const float red   = temperature <= 66.0f ? 255.0f : 329.698727446f * std::pow(temperature - 60.0f, -0.1332047592f);
    const float green = temperature <= 66.0f ? 99.4708025861f * std::log(temperature) - 161.1195681661f
                                             : 288.1221695283f * std::pow(temperature - 60.0f, -0.0755148492f);
    const float blue  = temperature >= 66.0f ? 255.0f
                      : temperature <= 19.0f ? 0.0f
                                             : 138.5177312231f * std::log(temperature - 10.0f) - 305.0447927307f;
    return { std::clamp(red, 0.0f, 255.0f) / 255.0f, std::clamp(green, 0.0f, 255.0f) / 255.0f, std::clamp(blue, 0.0f, 255.0f) / 255.0f, 1.0f };
  }

  Color Color::LerpHSV(const Color& from, const Color& to, float t) {
    ColorHSV start = from.ToHSV();
    ColorHSV end   = to.ToHSV();
    // 灰色（彩度 0）は色相を持たないので、相手の色相に合わせる
    if (start.s <= 0.0f) { start.h = end.h; }
    if (end.s <= 0.0f) { end.h = start.h; }
    // 色相環の近い方を回る
    float hueDelta = end.h - start.h;
    if (hueDelta > 0.5f) {
      hueDelta -= 1.0f;
    } else if (hueDelta < -0.5f) {
      hueDelta += 1.0f;
    }
    return ColorHSV(start.h + hueDelta * t, start.s + (end.s - start.s) * t, start.v + (end.v - start.v) * t, start.a + (end.a - start.a) * t).ToColor();
  }

  ColorHSV Color::ToHSV() const {
    const float maxValue = (std::max)({ r, g, b });
    const float minValue = (std::min)({ r, g, b });
    const float delta    = maxValue - minValue;
    float       hue      = 0.0f;
    if (delta > 0.0f) {
      // 一番大きい成分で、色相環を赤・緑・青の周りの 3 つに分ける
      if (maxValue == r) {
        hue = (g - b) / delta;
      } else if (maxValue == g) {
        hue = (b - r) / delta + 2.0f;
      } else {
        hue = (r - g) / delta + 4.0f;
      }
      hue /= 6.0f;
      if (hue < 0.0f) { hue += 1.0f; }
    }
    return { hue, maxValue > 0.0f ? delta / maxValue : 0.0f, maxValue, a };
  }

  Color ColorHSV::ToColor() const {
    // 色相環を 60 度ずつ 6 区間に分け、区間ごとに RGB のどれが最大・中間になるかが入れ替わる
    const float sector = (h - std::floor(h)) * 6.0f;
    const float chroma = v * s;
    const float middle = chroma * (1.0f - std::abs(std::fmod(sector, 2.0f) - 1.0f));
    const float base   = v - chroma;

    float red = 0.0f, green = 0.0f, blue = 0.0f;
    switch (static_cast<int>(sector)) {
    case 0:  red = chroma; green = middle; break;
    case 1:  red = middle; green = chroma; break;
    case 2:  green = chroma; blue = middle; break;
    case 3:  green = middle; blue = chroma; break;
    case 4:  red = middle; blue = chroma; break;
    default: red = chroma; blue = middle; break;
    }
    return { red + base, green + base, blue + base, a };
  }

#ifdef _DEBUG
  namespace {

    bool IsNear(const Color& actual, const Color& expected, float tolerance = 0.002f) {
      return std::abs(actual.r - expected.r) <= tolerance && std::abs(actual.g - expected.g) <= tolerance &&
             std::abs(actual.b - expected.b) <= tolerance && std::abs(actual.a - expected.a) <= tolerance;
    }

  } // namespace

  void RunColorSelfTest() {
    // HSV との往復。色相は範囲の外なら巡回する
    for (const Color& color : { Color::kRed, Color::kCyan, Color::kGray, Color(0.2f, 0.4f, 0.6f, 0.5f) }) {
      assert(IsNear(color.ToHSV().ToColor(), color));
    }
    assert(IsNear(Color::FromHSV(-0.25f, 1.0f, 1.0f), Color::FromHSV(0.75f, 1.0f, 1.0f)));
    assert(IsNear(Color::FromHSV(1.0f, 1.0f, 1.0f), Color::kRed));
    const ColorHSV yellow = Color::kYellow.ToHSV();
    assert(std::abs(yellow.h - 1.0f / 6.0f) < 0.001f && yellow.s == 1.0f && yellow.v == 1.0f);

    // 0〜255 と 16 進数。0〜1 の外は丸める
    assert(Color32::FromHex(0xFF8000).ToHex() == 0xFF8000);
    assert(IsNear(Color32(255, 128, 0), Color::FromHex(0xFF8000)));
    const Color32 bytes = Color(1.5f, 0.5f, -1.0f, 1.0f).ToColor32();
    assert(bytes.r == 255 && bytes.g == 128 && bytes.b == 0 && bytes.a == 255);

    // 緑から赤の真ん中は黄。灰色の側は相手の色相を使う
    assert(IsNear(Color::LerpHSV(Color::kGreen, Color::kRed, 0.5f), Color::kYellow));
    assert(IsNear(Color::LerpHSV(Color::kWhite, Color::kBlue, 0.5f), Color(0.5f, 0.5f, 1.0f)));

    assert(IsNear(Color::FromKelvin(6600.0f), Color::kWhite));
    assert(IsNear(Color::FromKelvin(2000.0f), Color(1.0f, 0.537f, 0.055f), 0.01f));

    // 段階が無ければ白。段階の順は問わず、範囲の外は端の色
    Gradient gradient;
    assert(IsNear(gradient.Evaluate(0.5f), Color::kWhite));
    gradient.keys = { { 1.0f, Color::kBlue }, { 0.0f, Color::kRed }, { 0.5f, Color::kGreen } };
    assert(IsNear(gradient.Evaluate(-1.0f), Color::kRed));
    assert(IsNear(gradient.Evaluate(0.25f), Color(0.5f, 0.5f, 0.0f)));
    assert(IsNear(gradient.Evaluate(0.5f), Color::kGreen));
    assert(IsNear(gradient.Evaluate(2.0f), Color::kBlue));

    const ColorHSV random = RandomEngine::GetInstance()->GetHueColor(0.5f, 0.8f).ToHSV();
    assert(std::abs(random.s - 0.5f) < 0.001f && std::abs(random.v - 0.8f) < 0.001f);
  }
#endif

} // namespace Tako
