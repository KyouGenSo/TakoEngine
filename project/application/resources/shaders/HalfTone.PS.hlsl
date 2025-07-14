#include "FullScreen.hlsli"

cbuffer HalfToneParam : register(b0)
{
    float dotSize;      // ドットのサイズ
    float contrast;     // コントラスト
    float2 screenSize; // スクリーンサイズ
}

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 元の色をサンプリング
    float4 originalColor = gTexture.Sample(gSampler, input.texCoord);
    
    // グレースケールに変換してドット生成の元となる明度を計算
    float luminance = dot(originalColor.rgb, float3(0.299, 0.587, 0.114));
    
    // スクリーン座標をドットサイズで分割してグリッドを作成
    float2 gridPosition = floor(input.texCoord * screenSize / dotSize) * dotSize;
    float2 gridTexCoord = gridPosition / screenSize;
    
    // グリッド中心からの距離を計算
    float2 pixelPosition = input.texCoord * screenSize;
    float2 gridCenterPosition = (gridPosition + dotSize * 0.5);
    float distanceFromCenter = length(pixelPosition - gridCenterPosition);
    
    // グリッド中心の明度を取得（グリッドサイズ全体の代表値として）
    float gridLuminance = dot(gTexture.Sample(gSampler, gridTexCoord).rgb, float3(0.299, 0.587, 0.114));
    
    // 明度に基づいてドットの半径を決定
    float dotRadius = (dotSize * 0.5) * gridLuminance * contrast;
    
    // ドットの中心からの距離でアルファ値を決定（アンチエイリアシング付き）
    float alpha = 1.0 - smoothstep(dotRadius - 1.0, dotRadius + 1.0, distanceFromCenter);
    
    // 最終的な色を計算（ハーフトーンのドットパターン）
    float3 finalColor = lerp(float3(1.0, 1.0, 1.0), originalColor.rgb, alpha);
    
    return float4(finalColor, originalColor.a);
}