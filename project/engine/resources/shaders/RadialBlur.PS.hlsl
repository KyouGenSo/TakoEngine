#include "FullScreen.hlsli"
#include "Common.hlsli"

struct Param
{
    float2 center;
    float blurWidth;
    int32_t sampleCount;
};

ConstantBuffer<Param> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 dir = input.texCoord - gParam.center;
    float4 outputColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int32_t sampleIndex = 0; sampleIndex < gParam.sampleCount; ++sampleIndex)
    {
        float2 sampleOffset = input.texCoord + dir * gParam.blurWidth * float(sampleIndex);
        outputColor += gTexture.Sample(gLinearClamp, sampleOffset);
    }

    return outputColor * rcp(float(gParam.sampleCount));
}