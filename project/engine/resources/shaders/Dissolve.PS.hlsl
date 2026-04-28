#include "FullScreen.hlsli"

struct DissolvePram
{
    float threshold;
    float edgeThickness;
    float4 edgeColor;
};

ConstantBuffer<DissolvePram> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);     // シーンのカラーテクスチャ
Texture2D<float> gMaskTexture : register(t1);  // マスクテクスチャ
Texture2D<float4> gBaseTexture : register(t2); // 下地のカラーテクスチャ
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;

	// 下地の色を取得
    float4 baseColor = gBaseTexture.Sample(gSampler, input.texCoord);

	// マスクの値を取得
    float mask = gMaskTexture.Sample(gSampler, input.texCoord);

    // マスクの値が閾値を超えている場合、下地の色を使用
    if (mask <= gParam.threshold)
    {
        return baseColor;
    }

    float edgeJudge = gParam.threshold + gParam.edgeThickness;
    if (gParam.threshold > 1.0f)
    {
        edgeJudge = 1.0f;
    }
    float edge = 1.0f - smoothstep(gParam.threshold, edgeJudge, mask);

	// シーンの色を取得
    output.color = gTexture.Sample(gSampler, input.texCoord);

    output.color.rgb += edge * gParam.edgeColor.rgb; // エッジの色を加算

    return output.color;

}