struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPos : POSITION0;
    float4 lightSpacePos : TEXCOORD1;  // ライト空間での位置
    float4 instanceColor : COLOR0;     // インスタンスごとのカラー
};

// シャドウマップ用定数バッファ
struct ShadowConstants
{
    float4x4 lightViewProj; // ライトビュープロジェクション行列
    float shadowBias;        // シャドウバイアス
    int enableShadow;        // シャドウの有効/無効
    float2 shadowMapSize;    // シャドウマップのサイズ（PCF用）
    float normalOffsetBias;  // 法線オフセットバイアス
    float pcfKernelSize;     // PCFカーネルサイズ（3, 5, 7, 9）
};