#include "FullScreen.hlsli"
#include "Common.hlsli"

Texture2D<float4> gTexture : register(t0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gLinearWrap, input.texCoord);
    output.color.a = 1.0f;
    return output.color;
}