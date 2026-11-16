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
cbuffer gTransformationMatrix : register(b0)
{
    float4x4 World;
    float4x4 WorldIT;
};

// 影の視点 1 つぶん（ShadowRenderer::ShadowCasterConstants と一致させること）
cbuffer gShadowCaster : register(b4)
{
    float4x4 lightViewProj;
    float pancake; // 1 なら near 面より手前の頂点を near 面で止める（平行光源）
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    
    // ワールド座標に変換
    float4 worldPos = mul(input.position, World);
    
    output.position = mul(worldPos, lightViewProj);

    // 平行光源は、影の範囲よりライト側にある物も影を落とすので、切らずに near 面へ押し付ける
    output.position.z = lerp(output.position.z, max(output.position.z, 0.0), pancake);
    
    // 深度値をテクスチャ座標として出力（デバッグ用）
    output.depth = output.position.z / output.position.w;

    return output;
}