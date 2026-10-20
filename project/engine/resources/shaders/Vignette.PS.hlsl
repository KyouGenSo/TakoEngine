#include "FullScreen.hlsli"
#include "Common.hlsli"

struct VignetteParam
{
    float power;
    float range;
    float3 color;
};

ConstantBuffer<VignetteParam> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gLinearWrap, input.texCoord);
    
    float2 correct = input.texCoord * (1.0f - input.texCoord.xy);
    
    float vignette = correct.x * correct.y * gParam.range;
    
    vignette = saturate(pow(vignette, gParam.power));

    output.color.rgb = lerp(output.color.rgb, gParam.color, 1.0f - vignette);
    
    return output.color;

}