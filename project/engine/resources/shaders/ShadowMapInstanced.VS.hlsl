// シャドウマップ生成用インスタンシング頂点シェーダー

struct VertexInput
{
    float4 position : POSITION0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float depth : TEXCOORD0;
};

// インスタンスデータ構造体（Object3dInstanced.VS.hlslと同じ）
struct InstanceData
{
    float4x4 world;              // ワールド変換行列
    float4x4 worldInvTranspose;  // ワールド逆転置行列（法線変換用）
    float4 color;                // インスタンス毎のカラー（シャドウ生成では使用しない）
    float padding[12];            // 16バイトアラインメント用（256バイトに調整）
};

// インスタンスバッファ（t5レジスタを使用）
StructuredBuffer<InstanceData> gInstanceData : register(t5);

// 影の視点 1 つぶん（ShadowRenderer::ShadowCasterConstants と一致させること）
cbuffer gShadowCaster : register(b4)
{
    float4x4 lightViewProj;
    float pancake; // 1 なら near 面より手前の頂点を near 面で止める（平行光源）
};

VertexOutput main(VertexInput input, uint instanceID : SV_InstanceID)
{
    VertexOutput output;
    
    // インスタンスのワールド行列を取得
    float4x4 worldMatrix = gInstanceData[instanceID].world;
    
    // ワールド座標に変換
    float4 worldPos = mul(input.position, worldMatrix);
    
    output.position = mul(worldPos, lightViewProj);

    // 平行光源は、影の範囲よりライト側にある物も影を落とすので、切らずに near 面へ押し付ける
    output.position.z = lerp(output.position.z, max(output.position.z, 0.0), pancake);
    
    // 深度値をテクスチャ座標として出力（デバッグ用）
    output.depth = output.position.z / output.position.w;

    return output;
}