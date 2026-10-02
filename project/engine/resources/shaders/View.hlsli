// 視点ごとの定数（Object3dBasic::ViewConstants と一致させること）
struct View
{
    float4x4 viewProj;
    float3 worldPos;
    float padding;
};
