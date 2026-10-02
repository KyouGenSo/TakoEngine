#include "SkyBox.hlsli"
#include "View.hlsli"

struct VertexShaderInput
{
    float4 position : POSITION;
};

struct TransformationMatrix
{
    float4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<View> gView : register(b1);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(mul(input.position, gTransformationMatrix.World), gView.viewProj).xyww;
    output.texcoord = input.position.xyz;
    return output;
}