#include "FullScreen.hlsli"

struct BWFilterParam
{
    float threshold;
};

ConstantBuffer<BWFilterParam> gBWFilterParam : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 輝度が threshold 以上なら白、未満なら黒の2値化フィルタ
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, input.texCoord);

    float luminance = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));

    float value = luminance >= gBWFilterParam.threshold ? 1.0f : 0.0f;

    return float4(value, value, value, 1.0f);
}
