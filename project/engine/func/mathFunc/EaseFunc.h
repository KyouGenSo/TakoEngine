#pragma once

namespace Tako {

  /// <summary>
  /// イージング関数ユーティリティ名前空間
  /// 使用例: Vec3::Lerp(start, end, Ease::OutQuad(t));
  /// </summary>
  namespace Ease {

    /// <summary>
    /// 線形（イージングなし）。f(t) = t
    /// </summary>
    float Linear(float t);

    // --- Quad（2次） ---
    /// <summary>
    /// 2次・加速。f(t) = t^2
    /// </summary>
    float InQuad(float t);
    /// <summary>
    /// 2次・減速。f(t) = 1 - (1 - t)^2
    /// </summary>
    float OutQuad(float t);
    /// <summary>
    /// 2次・加速→減速。t&lt;0.5: 2t^2 / それ以外: 1 - (-2t + 2)^2 / 2
    /// </summary>
    float InOutQuad(float t);

    // --- Cubic（3次） ---
    /// <summary>
    /// 3次・加速。f(t) = t^3
    /// </summary>
    float InCubic(float t);
    /// <summary>
    /// 3次・減速。f(t) = 1 - (1 - t)^3
    /// </summary>
    float OutCubic(float t);
    /// <summary>
    /// 3次・加速→減速。t&lt;0.5: 4t^3 / それ以外: 1 - (-2t + 2)^3 / 2
    /// </summary>
    float InOutCubic(float t);

    // --- Quart（4次） ---
    /// <summary>
    /// 4次・加速。f(t) = t^4
    /// </summary>
    float InQuart(float t);
    /// <summary>
    /// 4次・減速。f(t) = 1 - (1 - t)^4
    /// </summary>
    float OutQuart(float t);
    /// <summary>
    /// 4次・加速→減速。t&lt;0.5: 8t^4 / それ以外: 1 - (-2t + 2)^4 / 2
    /// </summary>
    float InOutQuart(float t);

    // --- Quint（5次） ---
    /// <summary>
    /// 5次・加速。f(t) = t^5
    /// </summary>
    float InQuint(float t);
    /// <summary>
    /// 5次・減速。f(t) = 1 - (1 - t)^5
    /// </summary>
    float OutQuint(float t);
    /// <summary>
    /// 5次・加速→減速。t&lt;0.5: 16t^5 / それ以外: 1 - (-2t + 2)^5 / 2
    /// </summary>
    float InOutQuint(float t);

    // --- Sine（正弦） ---
    /// <summary>
    /// 正弦・加速。f(t) = 1 - cos(t * π / 2)
    /// </summary>
    float InSine(float t);
    /// <summary>
    /// 正弦・減速。f(t) = sin(t * π / 2)
    /// </summary>
    float OutSine(float t);
    /// <summary>
    /// 正弦・加速→減速。f(t) = -(cos(π * t) - 1) / 2
    /// </summary>
    float InOutSine(float t);

    // --- Expo（指数） ---
    /// <summary>
    /// 指数・加速。t==0: 0 / それ以外: 2^(10t - 10)
    /// </summary>
    float InExpo(float t);
    /// <summary>
    /// 指数・減速。t==1: 1 / それ以外: 1 - 2^(-10t)
    /// </summary>
    float OutExpo(float t);
    /// <summary>
    /// 指数・加速→減速。端点 0/1、t&lt;0.5: 2^(20t - 10)/2 / それ以外: (2 - 2^(-20t + 10))/2
    /// </summary>
    float InOutExpo(float t);

    // --- Circ（円弧） ---
    /// <summary>
    /// 円弧・加速。f(t) = 1 - sqrt(1 - t^2)
    /// </summary>
    float InCirc(float t);
    /// <summary>
    /// 円弧・減速。f(t) = sqrt(1 - (t - 1)^2)
    /// </summary>
    float OutCirc(float t);
    /// <summary>
    /// 円弧・加速→減速。t&lt;0.5: (1 - sqrt(1 - (2t)^2))/2 / それ以外: (sqrt(1 - (-2t + 2)^2) + 1)/2
    /// </summary>
    float InOutCirc(float t);

    // --- Back（行き過ぎて戻る。範囲外オーバーシュートあり） ---
    /// <summary>
    /// バック・加速。c1=1.70158, c3=c1+1。f(t) = c3*t^3 - c1*t^2
    /// </summary>
    float InBack(float t);
    /// <summary>
    /// バック・減速。f(t) = 1 + c3*(t - 1)^3 + c1*(t - 1)^2
    /// </summary>
    float OutBack(float t);
    /// <summary>
    /// バック・加速→減速。c2 = c1 * 1.525
    /// </summary>
    float InOutBack(float t);

    // --- Elastic（弾性・減衰振動。範囲外オーバーシュートあり） ---
    /// <summary>
    /// 弾性・加速。端点 0/1、c4=2π/3。-2^(10t - 10) * sin((10t - 10.75) * c4)
    /// </summary>
    float InElastic(float t);
    /// <summary>
    /// 弾性・減速。端点 0/1、c4=2π/3。2^(-10t) * sin((10t - 0.75) * c4) + 1
    /// </summary>
    float OutElastic(float t);
    /// <summary>
    /// 弾性・加速→減速。端点 0/1、c5=2π/4.5
    /// </summary>
    float InOutElastic(float t);

    // --- Bounce（跳ね返り） ---
    /// <summary>
    /// バウンス・加速。f(t) = 1 - OutBounce(1 - t)
    /// </summary>
    float InBounce(float t);
    /// <summary>
    /// バウンス・減速。区分多項式（n1=7.5625, d1=2.75）
    /// </summary>
    float OutBounce(float t);
    /// <summary>
    /// バウンス・加速→減速。前半 (1 - OutBounce(1 - 2t))/2 / 後半 (1 + OutBounce(2t - 1))/2
    /// </summary>
    float InOutBounce(float t);

    // --- 多項式平滑（Hermite 系） ---
    /// <summary>
    /// スムーズステップ。f(t) = t^2 * (3 - 2t)。両端で傾き 0 の滑らかな S 字
    /// </summary>
    float SmoothStep(float t);
    /// <summary>
    /// スムーザーステップ。f(t) = 6t^5 - 15t^4 + 10t^3。両端で1次・2次微分が 0
    /// </summary>
    float SmootherStep(float t);

    // --- CubicBezier（カスタム制御点） ---
    /// <summary>
    /// カスタムイージング。始点(0,0)と終点(1,1)を制御点 (x1,y1),(x2,y2) で結んだ曲線をイージングとして使う
    /// （CSS の cubic-bezier と同じ。x が時間、y が進行度で、曲線上で x=t となる点の y を返す）。
    /// x1/x2 は [0,1] に clamp。y1/y2 は自由で、範囲外にすると行き過ぎて戻る動きも作れる
    /// </summary>
    float CubicBezier(float t, float x1, float y1, float x2, float y2);

  } // namespace Ease

} // namespace Tako
