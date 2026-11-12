#include "Sprite.hlsli"
#include "Common.hlsli"

struct Material
{
    float4 color;
    float4 outlineColor;
    float2 unitRange; // SDF の値 0〜1 が表す距離（アトラスの UV）
    float outline;    // 縁取りの太さ（SDF の値。0 なら縁取りなし）
    float padding;
};

ConstantBuffer<Material> gMaterial : register(b0);

// 字ごとの距離場（R）。0.5 が輪郭で、内側ほど大きい
Texture2D<float4> gTexture : register(t0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float distance = gTexture.Sample(gLinearClamp, input.texcoord).r;

    // SDF の値 0〜1 が画面で何 px にあたるか。1px より狭くなる縮小では縁が消えるので下限を置く
    float2 screenTexSize = 1.0f / fwidth(input.texcoord);
    float pxRange = max(0.5f * dot(gMaterial.unitRange, screenTexSize), 1.0f);

    // 輪郭をまたぐ 1px でなめらかに 0〜1 にする。body は縁取りの外側の縁まで含めた形
    float face = saturate((distance - 0.5f) * pxRange + 0.5f);
    float body = saturate((distance - 0.5f + gMaterial.outline) * pxRange + 0.5f);

    // 形の中で字の本体が占める割合で色を混ぜる（縁取りが無ければ face == body なので字の色だけになる）
    float4 color = lerp(gMaterial.outlineColor, gMaterial.color, saturate(face / max(body, 0.0001f)));
    color.a *= body;

    output.color = color;
    return output;
}
