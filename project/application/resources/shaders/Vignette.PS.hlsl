#include "FullScreen.hlsli"

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
    
    float vignette = correct.x * correct.y * 15.0f;
    
    vignette = saturate(pow(vignette, 0.8f));
    
    output.color.rgb *= vignette;
    
    return output.color;

}