// 視点ごとの定数（Renderer::ViewConstants と一致させること）
struct View
{
    float4x4 viewProj;
    float3 worldPos;
    float time;                // ゲーム開始からの秒数
    float4x4 viewProjInverse;  // NDC → ワールド
    float4x4 projInverse;      // NDC → ビュー空間
    float2 screenSize;         // 描画先の画素数
    float2 padding;
};
