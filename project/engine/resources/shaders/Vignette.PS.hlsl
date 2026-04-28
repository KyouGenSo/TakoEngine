#include "FullScreen.hlsli"

struct VignetteParam
{
    float power;
    float range;
    float3 color;
};

ConstantBuffer<VignetteParam> gVignetteParam : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    
    float2 correct = input.texCoord * (1.0f - input.texCoord.xy);
    
    float vignette = correct.x * correct.y * gVignetteParam.range;
    
    vignette = saturate(pow(vignette, gVignetteParam.power));

    output.color.rgb = lerp(output.color.rgb, gVignetteParam.color, 1.0f - vignette); // vignetteÇ≈êFÇï‚ä‘
    
    return output.color;

}