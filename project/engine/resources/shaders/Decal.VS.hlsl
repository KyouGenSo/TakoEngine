/// ============================================ ///
/// Decal.VS.hlsl - Projective Decal 頂点シェーダー
/// 単位キューブをクリップ空間に変換
/// ============================================ ///

#include "Decal.hlsli"

ConstantBuffer<ViewData> gViewData : register(b0);
ConstantBuffer<DecalData> gDecalData : register(b1);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.pos = mul(input.pos, gDecalData.decalWVP);
    return output;
}
