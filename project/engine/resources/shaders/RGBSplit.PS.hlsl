#include "FullScreen.hlsli"
#include "Common.hlsli"

Texture2D<float4> gTexture : register(t0);

// RGB Splitパラメータ
struct RGBSplitParam
{
    float2 redOffset;
    float2 greenOffset;
    float2 blueOffset;
    float intensity;
};

ConstantBuffer<RGBSplitParam> gParam : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 texCoord = input.texCoord;

  // チャンネルごとに異なるオフセットでサンプリングし色収差を作る
    float r = gTexture.Sample(gLinearClamp, texCoord + gParam.redOffset * gParam.intensity).r;
    float g = gTexture.Sample(gLinearClamp, texCoord + gParam.greenOffset * gParam.intensity).g;
    float b = gTexture.Sample(gLinearClamp, texCoord + gParam.blueOffset * gParam.intensity).b;
    float a = gTexture.Sample(gLinearClamp, texCoord).a;

    return float4(r, g, b, a);
}