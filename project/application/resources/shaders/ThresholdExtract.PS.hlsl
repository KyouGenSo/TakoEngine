#include "FullScreen.hlsli"

cbuffer BloomParam : register(b0)
{
    float intensity;
    float threshold;
    float sigma;
    float2 direction; // x方向: float2(1,0), y方向: float2(0,1)
    float2 texelSize; // 1/width, 1/height
    int sampleCount; // サンプル数
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 BloomExtract(float2 texcoord)
{
    float4 color = gTexture.Sample(gSampler, texcoord);
    // 閾値の範囲を定義
    float minThreshold = threshold - 0.1f;
    float maxThreshold = threshold;
    // smoothstepで滑らかな閾値適用
    float brightness = max(color.r, max(color.g, color.b));
    float factor = smoothstep(minThreshold, maxThreshold, brightness);
    return color * factor;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, input.texCoord);
    
    // 輝度計算（RGB→輝度への変換）
    float brightness = dot(color.rgb, float3(0.299, 0.587, 0.114));
    
    // 閾値以上の明るさのみ抽出
    float contribution = max(0, brightness - threshold);
    
    // 結果を出力
    return float4(color.rgb * contribution, 1.0);
}