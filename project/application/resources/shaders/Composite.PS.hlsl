
#include "FullScreen.hlsli"

Texture2D<float4> effectTexture : register(t0); // ポストエフェクト適用済み
Texture2D<float4> nonEffectTexture : register(t1); // ポストエフェクト非適用
SamplerState smp : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 effectColor = effectTexture.Sample(smp, input.texCoord);
    float4 nonEffectColor = nonEffectTexture.Sample(smp, input.texCoord);
    
    // アルファブレンディングによる合成
    return float4(
        lerp(effectColor.rgb, nonEffectColor.rgb, nonEffectColor.a),
        max(effectColor.a, nonEffectColor.a)
    );
}