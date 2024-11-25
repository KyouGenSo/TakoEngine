#include "Sprite.hlsli"

struct Material
{
    float32_t4 color;
    float32_t4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    output.color = texColor * gMaterial.color;
    
    return output;
}