#include "FullScreen.hlsli"
#include "Common.hlsli"

struct BWFilterParam
{
    float threshold;
};

ConstantBuffer<BWFilterParam> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);

// 輝度が threshold 以上なら白、未満なら黒の2値化フィルタ
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gLinearWrap, input.texCoord);

    float luminance = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));

    float value = luminance >= gParam.threshold ? 1.0f : 0.0f;

    return float4(value, value, value, 1.0f);
}
