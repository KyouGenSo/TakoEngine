#include "FullScreen.hlsli"

cbuffer BloomParam : register(b0)
{
    float intensity;
    float threshold;
    float sigma;
    float2 direction; // x方向: float2(1,0), y方向: float2(0,1)
    float2 texelSize; // 1/width, 1/height
    int sampleCount;  // サンプル数
    int iteration;
};

Texture2D<float4> gHighLumTex : register(t0);
Texture2D<float4> gShrinkTex : register(t1);
SamplerState gSampler : register(s0);

//float CalcGaussianWeight(float x, float sigma)
//{
//    return exp(-(x * x) / (2.0 * sigma * sigma));
//}

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy, float4 rect)
{
    float4 ret = tex.Sample(smp, uv);
    float4 blurColor = float4(0, 0, 0, 0);

    float weights[5][5] =
    {
        { 1 / 273.0, 4 / 273.0, 7 / 273.0, 4 / 273.0, 1 / 273.0 },
        { 4 / 273.0, 16 / 273.0, 26 / 273.0, 16 / 273.0, 4 / 273.0 },
        { 7 / 273.0, 26 / 273.0, 41 / 273.0, 26 / 273.0, 7 / 273.0 },
        { 4 / 273.0, 16 / 273.0, 26 / 273.0, 16 / 273.0, 4 / 273.0 },
        { 1 / 273.0, 4 / 273.0, 7 / 273.0, 4 / 273.0, 1 / 273.0 }
    };

    float offsets[5] = { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f };

    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            float2 offset = float2(offsets[i] * dx, offsets[j] * dy);
            float2 sampleUV = uv + offset;

            sampleUV.x = clamp(sampleUV.x, rect.x + dx * 0.5, rect.z - dx * 0.5);
            sampleUV.y = clamp(sampleUV.y, rect.y + dy * 0.5, rect.w - dy * 0.5);

            blurColor += tex.Sample(smp, sampleUV) * weights[i][j];
        }
    }

    return float4(blurColor.rgb, ret.a);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    //float4 color = float4(0, 0, 0, 0);
    //float totalWeight = 0.0;
    
    //// 指定方向のブラー
    //for (int i = -sampleCount; i <= sampleCount; i++)
    //{
    //    float weight = CalcGaussianWeight(float(i), sigma);
    //    color += gTexture.Sample(gSampler, input.texCoord + direction * texelSize * i) * weight;
    //    totalWeight += weight;
    //}
    
    //return color / totalWeight;

    float w, h, levels;
    gHighLumTex.GetDimensions(0, w, h, levels);

    float dx = 1.0f / w;
    float dy = 1.0f / h;

    float4 bloomAccum = float4(0, 0, 0, 0);
    float2 uvSize = float2(1, 0.5);
    float2 uvOffset = float2(0, 0);

    for (int i = 0; i < iteration; ++i)
    {
        bloomAccum += Get5x5GaussianBlur(gShrinkTex, gSampler, input.texCoord * uvSize + uvOffset, dx, dy, float4(uvOffset, uvOffset + uvSize));
        uvOffset.y += uvSize.y;
        uvSize *= 0.5f;
    }

    float4 bloomColor = Get5x5GaussianBlur(gHighLumTex, gSampler, input.texCoord, dx, dy, float4(0, 0, 1, 1)) + saturate(bloomAccum);
    return bloomColor;
}