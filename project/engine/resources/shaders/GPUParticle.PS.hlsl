#include "Particle.hlsli"
#include "Common.hlsli"

Texture2D<float4> gTexture : register(t0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 textureColor = gTexture.Sample(gLinearWrap, input.texcoord);
    
    output.color = textureColor * input.color;
    
    if (output.color.a <= 0.0f)
    {
        discard;
    };
    
    return output;
}