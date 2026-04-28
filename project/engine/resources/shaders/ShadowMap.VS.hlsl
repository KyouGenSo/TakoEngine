// シャドウマップ生成用頂点シェーダー

struct VertexInput
{
    float4 position : POSITION0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float depth : TEXCOORD0;
};

// ワールド変換行列
cbuffer TransformationMatrix : register(b0)
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldIT;
};

// ライトビュープロジェクション行列
cbuffer ShadowTransform : register(b4)
{
    float4x4 lightViewProj;
    float shadowBias;
    int enableShadow;
    float2 shadowMapSize;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    
    // ワールド座標に変換
    float4 worldPos = mul(input.position, World);
    
    // ライトビュープロジェクション座標に変換
    output.position = mul(worldPos, lightViewProj);
    
    // 深度値をテクスチャ座標として出力（デバッグ用）
    output.depth = output.position.z / output.position.w;
    
    // 深度バイアスを適用（頂点シェーダーでの実装）
    // 通常はラスタライザーステートで設定するが、手動でも調整可能
    output.position.z += shadowBias * output.position.w;
    
    return output;
}