#include "FullScreen.hlsli"
#include "Random.hlsli"
#include "Common.hlsli"

struct Param
{
    float time;
};

ConstantBuffer<Param> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gLinearWrap, input.texCoord);

    float random = rand2dTo1d(input.texCoord * gParam.time);

    output.color.rgb *= random;

    return output.color;

}