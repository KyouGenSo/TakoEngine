#include "Object3d.hlsli"
#include "View.hlsli"

struct TransformationMatrix
{
    float4x4 World;
    float4x4 WorldIT;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<View> gView : register(b2);
ConstantBuffer<ShadowConstants> gShadowConstants : register(b4);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main( VertexShaderInput input )
{
    VertexShaderOutput output;
    float4 worldPos = mul(input.pos, gTransformationMatrix.World);
    output.pos = mul(worldPos, gView.viewProj);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldIT));
    output.worldPos = worldPos.xyz;

    // ライト空間での位置を計算
    output.lightSpacePos = mul(worldPos, gShadowConstants.lightViewProj);
    
    return output;
}