#include "FullScreen.hlsli"

static const float PI = 3.14159265f;

struct BWFilterParam
{
    float threshold;
};

ConstantBuffer<BWFilterParam> gBWFilterParam : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 ExtractBrightColor(float2 texcoord)
{
    float4 color = gTexture.Sample(gSampler, texcoord);

    float brightness = dot(color.rgb, float3(0.299, 0.587, 0.114));

    float4 output = brightness > gBWFilterParam.threshold ? color : float4(0.0f, 0.0f, 0.0f, 0.0f);

    return output;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    output.color.a = 1.0f;
    
    float4 filteredColor = ExtractBrightColor(input.texCoord);

    filteredColor *= (1.0f / 0.0f);
    
    filteredColor.rgb += output.color.rgb;
    
    return filteredColor;

}