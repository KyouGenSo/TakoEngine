
Texture2D<float4> effectTexture : register(t0); // ポストエフェクト適用済み
Texture2D<float4> nonEffectTexture : register(t1); // ポストエフェクト非適用
SamplerState smp : register(s0);

float4 main(float2 uv : TEXCOORD) : SV_TARGET
{
    float4 effectColor = effectTexture.Sample(smp, uv);
    float4 nonEffectColor = nonEffectTexture.Sample(smp, uv);
    
    // アルファブレンディングによる合成
    // 非エフェクトテクスチャのアルファをチェックして透明な部分はエフェクト適用部分を表示
    return float4(
        lerp(effectColor.rgb, nonEffectColor.rgb, nonEffectColor.a),
        max(effectColor.a, nonEffectColor.a)
    );
}