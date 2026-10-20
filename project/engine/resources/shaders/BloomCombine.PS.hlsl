#include "FullScreen.hlsli"
#include "Common.hlsli"

cbuffer gParam : register(b0)
{
    float intensity;
};

Texture2D<float4> gTexture : register(t0); // 元画像
Texture2D<float4> gBloomTexture : register(t1); // ブルーム画像

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 baseColor = gTexture.Sample(gLinearWrap, input.texCoord);
    float4 bloomColor = gBloomTexture.Sample(gLinearWrap, input.texCoord);

    // 非線形強調
    bloomColor.rgb = pow(bloomColor.rgb, 0.8) * intensity * 1.5;

    // 元画像とブルーム効果を加算合成
    float3 result = baseColor.rgb + bloomColor.rgb;
    
    // トーンマッピング的な処理で自然な明るさに戻す
    //result = result / (1.0 + result);
    
    return float4(result, 1.0f);
}