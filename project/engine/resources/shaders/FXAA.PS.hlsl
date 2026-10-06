#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// FXAA (Lottes) の軽量版。輝度勾配に垂直な方向へサンプルを伸ばして段差をぼかす
static const float kReduceMin = 1.0f / 128.0f;
static const float kReduceMul = 1.0f / 8.0f;
static const float kSpanMax = 8.0f;  ///< 探索距離の上限（px）
static const float3 kLuma = float3(0.299f, 0.587f, 0.114f);

float3 SampleRGB(float2 uv)
{
    return gTexture.SampleLevel(gSampler, uv, 0.0f).rgb;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 size;
    gTexture.GetDimensions(size.x, size.y);
    const float2 texel = 1.0f / size;
    const float2 uv = input.texCoord;

    const float4 center = gTexture.SampleLevel(gSampler, uv, 0.0f);
    const float lumaNW = dot(SampleRGB(uv + float2(-1.0f, -1.0f) * texel), kLuma);
    const float lumaNE = dot(SampleRGB(uv + float2(1.0f, -1.0f) * texel), kLuma);
    const float lumaSW = dot(SampleRGB(uv + float2(-1.0f, 1.0f) * texel), kLuma);
    const float lumaSE = dot(SampleRGB(uv + float2(1.0f, 1.0f) * texel), kLuma);
    const float lumaM = dot(center.rgb, kLuma);

    const float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    const float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // 勾配 (E-W, S-N) を 90 度回した向き = エッジに沿う向き
    float2 dir = float2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
    const float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * kReduceMul), kReduceMin);
    const float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -kSpanMax, kSpanMax) * texel;

    const float3 rgbA = 0.5f * (SampleRGB(uv + dir * (1.0f / 3.0f - 0.5f)) + SampleRGB(uv + dir * (2.0f / 3.0f - 0.5f)));
    const float3 rgbB = rgbA * 0.5f + 0.25f * (SampleRGB(uv - dir * 0.5f) + SampleRGB(uv + dir * 0.5f));

    // 広く取った rgbB が周囲の輝度範囲を外れたら別のエッジを跨いでいるので狭い rgbA を使う
    const float lumaB = dot(rgbB, kLuma);
    const float3 rgb = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
    return float4(rgb, center.a);
}
