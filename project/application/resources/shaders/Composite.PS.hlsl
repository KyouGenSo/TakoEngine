
#include "FullScreen.hlsli"

Texture2D<float4> effectTexture : register(t0); // ポストエフェクト適用済み
Texture2D<float4> nonEffectTexture : register(t1); // ポストエフェクト非適用
SamplerState smp : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 effectColor = effectTexture.Sample(smp, input.texCoord);
    float4 nonEffectColor = nonEffectTexture.Sample(smp, input.texCoord);

    // 各色チャンネルごとに、より大きい値を採用
    float3 finalColor = max(effectColor.rgb, nonEffectColor.rgb);
    
    return float4(effectColor.rgb + nonEffectColor.rgb, 1.0);
}