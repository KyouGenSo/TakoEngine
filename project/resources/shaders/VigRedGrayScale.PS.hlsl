#include "FullScreen.hlsli"

struct VignetteParam
{
    float power;
    float range;
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
    
    float2 correct = input.texCoord * (1.0 - input.texCoord.xy);
    
    float vignette = correct.x * correct.y * gVignetteParam.range;
    
    vignette = saturate(pow(vignette, gVignetteParam.power));
    
     // �Â��Ȃ镔���ɐԐF��ǉ�
    float3 redTint = float3(1.0f, 0.0f, 0.0f); // �ԐF
    output.color.rgb = dot(output.color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    output.color.rgb = lerp(output.color.rgb, redTint, 1.0f - vignette); // vignette�ŐF����
    
    return output.color;

}