#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldIT;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
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
    output.pos = mul( input.pos, gTransformationMatrix.WVP );
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldIT));
    output.worldPos = mul( input.pos, gTransformationMatrix.World ).xyz;
    
    // ライト空間での位置を計算
    float4 worldPos = mul(input.pos, gTransformationMatrix.World);
    output.lightSpacePos = mul(worldPos, gShadowConstants.lightViewProj);
    
    return output;
}