// シーンの色（リニアの HDR）を画面の色にする。露出 → トーンマップ → sRGB への変換 → 縞を消す揺らぎ の順（ToneMapToDisplay が描く）
#include "FullScreen.hlsli"
#include "Common.hlsli"

// C++ の ToneMapToDisplay の定数と同じ並び
struct ToneMapParam
{
    float exposureScale;  // 2 の EV 乗
    uint mode;            // ToneMapping（0: None、1: ACES）
    float2 padding;
};

ConstantBuffer<ToneMapParam> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);

// Krzysztof Narkowicz の ACES の近似式。入力はリニア、出力は 0〜1
float3 ACESFilm(float3 x)
{
    return saturate((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    const float4 color = gTexture.Sample(gLinearClamp, input.texCoord);
    float3 rgb = color.rgb * gParam.exposureScale;
    rgb = gParam.mode == 1 ? ACESFilm(rgb) : saturate(rgb);
    rgb = LinearToSRGB(rgb);

    // 8 bit の描き先へ丸めたときに、なだらかな所（空・光のにじみ）が縞にならないよう、1 階調に満たない揺らぎを足す（Interleaved Gradient Noise）
    const float noise = frac(52.9829189f * frac(dot(input.position.xy, float2(0.06711056f, 0.00583715f))));
    return float4(rgb + (noise - 0.5f) / 255.0f, color.a);
}
