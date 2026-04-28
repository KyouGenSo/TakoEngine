#include "FullScreen.hlsli"

struct Param
{
    float2 center;
    float blurWidth;
    int32_t sampleCount;
};

ConstantBuffer<Param> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 dir = input.texCoord - gParam.center;
    float3 outputColor = float3(0.0f, 0.0f, 0.0f);

    for (int32_t sampleIndex = 0; sampleIndex < gParam.sampleCount; ++sampleIndex)
    {
        float2 sampleOffset = input.texCoord + dir * gParam.blurWidth * float(sampleIndex);
        outputColor.rgb += gTexture.Sample(gSampler, sampleOffset).rgb;
    }

    outputColor *= rcp(float(gParam.sampleCount));

    PixelShaderOutput output;
    output.color.rgb = outputColor;
    output.color.a = 1.0f; // Alpha channel is set to 1.0 (opaque)

    return output.color;
}