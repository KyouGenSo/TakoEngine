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
    float value = dot(output.color.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    output.color.rgb = float32_t3(value, value, value);
    return output.color;

}