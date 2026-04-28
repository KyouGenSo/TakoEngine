#include "FullScreen.hlsli"

cbuffer HalfToneParam : register(b0)
{
    float dotSize;          // ドットのサイズ
    float contrast;         // コントラスト
    float angle;            // ドットグリッドの回転角度（ラジアン）
    int dotPattern;         // ドットパターン (0=円, 1=四角, 2=ダイヤモンド)
    float2 screenSize;      // スクリーンサイズ
    int colorMode;          // カラーモード (0=モノクロ, 1=CMYK風)
    float threshold;        // 閾値調整
    float padding;          // パディング
}

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 2D回転行列
float2x2 GetRotationMatrix(float angle)
{
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
    return float2x2(cosAngle, -sinAngle, sinAngle, cosAngle);
}

// 円形ドットパターン
float CircleDot(float2 position, float radius)
{
    float distance = length(position);
    return 1.0 - smoothstep(radius - 1.0, radius + 1.0, distance);
}

// 四角形ドットパターン
float SquareDot(float2 position, float size)
{
    float2 absPos = abs(position);
    float maxDist = max(absPos.x, absPos.y);
    return 1.0 - smoothstep(size - 1.0, size + 1.0, maxDist);
}

// ダイヤモンド形ドットパターン
float DiamondDot(float2 position, float size)
{
    float distance = abs(position.x) + abs(position.y);
    return 1.0 - smoothstep(size - 1.0, size + 1.0, distance);
}

// CMYK分離を模擬したカラーハーフトーン
float3 CMYKHalftone(float4 originalColor, float2 texCoord, float2 screenSize, float dotSize, float contrast)
{
    float3 result = float3(1.0, 1.0, 1.0);
    
    // C（シアン）、M（マゼンタ）、Y（イエロー）チャンネルを個別に処理
    float3 cmy = 1.0 - originalColor.rgb;
    
    // 各チャンネルごとに異なる角度でハーフトーン処理
    float angles[3] = { 0.261799, 1.308997, 0.785398 }; // 15°, 75°, 45°
    
    for (int i = 0; i < 3; i++)
    {
        float2x2 rotMatrix = GetRotationMatrix(angles[i]);
        float2 rotatedTexCoord = mul(texCoord - 0.5, rotMatrix) + 0.5;
        
        float2 gridPosition = floor(rotatedTexCoord * screenSize / dotSize) * dotSize;
        float2 gridTexCoord = gridPosition / screenSize;
        
        float2 pixelPosition = rotatedTexCoord * screenSize;
        float2 gridCenterPosition = (gridPosition + dotSize * 0.5);
        float2 relativePosition = pixelPosition - gridCenterPosition;
        
        float channelValue = cmy[i];
        float dotRadius = (dotSize * 0.5) * channelValue * contrast;
        
        float alpha = CircleDot(relativePosition, dotRadius);
        
        // チャンネルごとの色を適用
        float3 channelColor = float3(1.0, 1.0, 1.0);
        channelColor[i] = 1.0 - alpha;
        
        result *= channelColor;
    }
    
    return result;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 元の色をサンプリング
    float4 originalColor = gTexture.Sample(gSampler, input.texCoord);
    
    // カラーモードに応じて処理を分岐
    if (colorMode == 1) // CMYK風カラーハーフトーン
    {
        float3 cmykResult = CMYKHalftone(originalColor, input.texCoord, screenSize, dotSize, contrast);
        return float4(cmykResult, originalColor.a);
    }
    else // 従来のモノクロハーフトーン
    {
        // グレースケールに変換してドット生成の元となる明度を計算
        float luminance = dot(originalColor.rgb, float3(0.299, 0.587, 0.114));
        
        // 閾値調整を適用
        luminance = saturate((luminance - threshold) / (1.0 - threshold));
        
        // 回転行列を適用
        float2x2 rotMatrix = GetRotationMatrix(angle);
        float2 rotatedTexCoord = mul(input.texCoord - 0.5, rotMatrix) + 0.5;
        
        // スクリーン座標をドットサイズで分割してグリッドを作成
        float2 gridPosition = floor(rotatedTexCoord * screenSize / dotSize) * dotSize;
        float2 gridTexCoord = gridPosition / screenSize;
        
        // グリッド中心からの相対位置を計算
        float2 pixelPosition = rotatedTexCoord * screenSize;
        float2 gridCenterPosition = (gridPosition + dotSize * 0.5);
        float2 relativePosition = pixelPosition - gridCenterPosition;
        
        // グリッド中心の明度を取得（グリッドサイズ全体の代表値として）
        float gridLuminance = dot(gTexture.Sample(gSampler, gridTexCoord).rgb, float3(0.299, 0.587, 0.114));
        gridLuminance = saturate((gridLuminance - threshold) / (1.0 - threshold));
        
        // 明度に基づいてドットのサイズを決定（暗い部分ほど大きなドット）
        float dotRadius = (dotSize * 0.5) * (1.0 - gridLuminance) * contrast;
        
        // ドットパターンに応じてアルファ値を計算
        float alpha;
        switch (dotPattern)
        {
            case 1: // 四角形
                alpha = SquareDot(relativePosition, dotRadius);
                break;
            case 2: // ダイヤモンド
                alpha = DiamondDot(relativePosition, dotRadius);
                break;
            default: // 円形
                alpha = CircleDot(relativePosition, dotRadius);
                break;
        }
        
        // 最終的な色を計算（ハーフトーンのドットパターン）
        float3 finalColor = lerp(originalColor.rgb, float3(0.0, 0.0, 0.0), alpha);
        
        return float4(finalColor, originalColor.a);
    }
}