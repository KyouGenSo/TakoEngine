#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// RGB Splitパラメータ
struct RGBSplitParam
{
    float2 redOffset;
    float2 greenOffset;
    float2 blueOffset;
    float intensity;
};

// CBV(Constant Buffer View)
ConstantBuffer<RGBSplitParam> gRGBSplitParam : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
  // 元のテクスチャ座標
    float2 texCoord = input.texCoord;
  
  // 各チャンネルのサンプリング位置をオフセットする
    float r = gTexture.Sample(gSampler, texCoord + gRGBSplitParam.redOffset * gRGBSplitParam.intensity).r;
    float g = gTexture.Sample(gSampler, texCoord + gRGBSplitParam.greenOffset * gRGBSplitParam.intensity).g;
    float b = gTexture.Sample(gSampler, texCoord + gRGBSplitParam.blueOffset * gRGBSplitParam.intensity).b;
    float a = gTexture.Sample(gSampler, texCoord).a;
  
  // 結果を組み合わせる
    return float4(r, g, b, a);
}