#include "SkyBox.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct Material
{
    float4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);

TextureCube<float32_t4> gSkyBoxTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 texColor = gSkyBoxTexture.Sample(gSampler, input.texcoord);
    output.color = texColor * gMaterial.color;
    return output;
}