#include "Particle.hlsli"

// リソースバインディング
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ピクセルシェーダー出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ピクセルシェーダーメイン関数
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャからカラーを取得
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // パーティクルの色と乗算
    output.color = textureColor * input.color;
    
    // アルファ値が0以下なら描画しない
    if (output.color.a <= 0.0f)
    {
        discard;
    };
    
    return output;
}