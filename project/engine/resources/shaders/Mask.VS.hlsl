// ObjectMask へ描画物の maskId を書くパスの頂点シェーダー
#include "View.hlsli"

struct TransformationMatrix
{
    float4x4 World;
    float4x4 WorldIT;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<View> gView : register(b2);

float4 main(float4 position : POSITION0) : SV_POSITION
{
    return mul(mul(position, gTransformationMatrix.World), gView.viewProj);
}
