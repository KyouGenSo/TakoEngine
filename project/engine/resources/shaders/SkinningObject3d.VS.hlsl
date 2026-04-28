#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldIT;
};

struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceMatrixIT;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

StructuredBuffer<Well> gMatrixPalette : register(t0);

struct VertexShaderInput
{
    float4 pos : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

struct Skinned
{
    float4 pos;
    float3 normal;
};

Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;

    // à íuÇÃïœä∑
    skinned.pos = mul(input.pos, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.pos += mul(input.pos, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.pos += mul(input.pos, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.pos += mul(input.pos, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.pos.w = 1.0f;
    
    // ñ@ê¸ÇÃïœä∑
    skinned.normal = mul(input.normal, (float3x3) gMatrixPalette[input.index.x].skeletonSpaceMatrixIT) * input.weight.x;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.y].skeletonSpaceMatrixIT) * input.weight.y;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.z].skeletonSpaceMatrixIT) * input.weight.z;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.w].skeletonSpaceMatrixIT) * input.weight.w;
    skinned.normal = normalize(skinned.normal);
    
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    Skinned skinned = Skinning(input);
    
    output.pos = mul(skinned.pos, gTransformationMatrix.WVP);
    output.worldPos = mul(skinned.pos, gTransformationMatrix.World).xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinned.normal, (float3x3) gTransformationMatrix.WorldIT));
    
    return output;
}