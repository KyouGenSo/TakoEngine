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

// シャドウ変換行列
cbuffer ShadowTransform : register(b4)
{
    float4x4 lightViewProj;
    float shadowBias;
    int enableShadow;
    float2 shadowMapSize;
    float normalOffsetBias;
    float pcfKernelSize;
    float padding[2];
};

VertexOutput main(VertexInput input, uint instanceID : SV_InstanceID)
{
    VertexOutput output;
    
    // インスタンスのワールド行列を取得
    float4x4 worldMatrix = gInstanceData[instanceID].world;
    
    // ワールド座標に変換
    float4 worldPos = mul(input.position, worldMatrix);
    
    // ライトビュープロジェクション座標に変換
    output.position = mul(worldPos, lightViewProj);
    
    // 深度値をテクスチャ座標として出力（デバッグ用）
    output.depth = output.position.z / output.position.w;
    
    // 深度バイアスを適用
    output.position.z += shadowBias * output.position.w;
    
    return output;
}