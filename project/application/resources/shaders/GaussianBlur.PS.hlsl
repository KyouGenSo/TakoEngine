#include "FullScreen.hlsli"

cbuffer BloomParam : register(b0)
{
    float intensity;
    float threshold;
    float sigma;
    float2 direction; // x方向: float2(1,0), y方向: float2(0,1)
    float2 texelSize; // 1/width, 1/height
    int sampleCount;  // サンプル数
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float CalcGaussianWeight(float x, float sigma)
{
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0.0;
    
    // 指定方向のブラー
    for (int i = -sampleCount; i <= sampleCount; i++)
    {
        float weight = CalcGaussianWeight(float(i), sigma);
        color += gTexture.Sample(gSampler, input.texCoord + direction * texelSize * i) * weight;
        totalWeight += weight;
    }
    
    return color / totalWeight;
}