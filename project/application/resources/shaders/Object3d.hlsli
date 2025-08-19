struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPos : POSITION0;
    float4 lightSpacePos : TEXCOORD1;  // ライト空間での位置
};

// シャドウマップ用定数バッファ
cbuffer ShadowConstants : register(b4)
{
    matrix lightViewProj;    // ライトビュープロジェクション行列
    float shadowBias;        // シャドウバイアス
    int enableShadow;        // シャドウの有効/無効
    float2 shadowMapSize;    // シャドウマップのサイズ（PCF用）
};