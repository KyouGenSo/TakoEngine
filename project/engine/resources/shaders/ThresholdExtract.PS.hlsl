#include "FullScreen.hlsli"

cbuffer Param : register(b0)
{
    float threshold;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, input.texCoord);
    
    // 輝度計算（RGB→輝度への変換）
    float brightness = dot(color.rgb, float3(0.299, 0.587, 0.114));
    
    // 閾値以上の明るさのみ抽出
    float4 output = brightness > threshold ? color : float4(0.0f, 0.0f, 0.0f, 0.0f);

    return output;

}