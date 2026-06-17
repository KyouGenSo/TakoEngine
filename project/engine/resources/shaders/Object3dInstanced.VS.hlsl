#include "Object3dInstanced.hlsli"

struct InstanceData
{
    float4x4 World;
    float4x4 WorldInvTranspose;
    float4 Color;
    float3 Padding[4]; // 256バイトアラインメント
};

struct ViewProjection
{
    float4x4 viewProj;
};

StructuredBuffer<InstanceData> gInstanceData : register(t5);
ConstantBuffer<ViewProjection> gViewProjection : register(b0);
ConstantBuffer<ShadowConstants> gShadowConstants : register(b4);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    
    InstanceData instance = gInstanceData[instanceID];
    
    float4 worldPos = mul(input.pos, instance.World);
    output.worldPos = worldPos.xyz;
    
    output.pos = mul(worldPos, gViewProjection.viewProj);
    
    // 法線変換（ワールド逆転置行列を使用）
    output.normal = normalize(mul(input.normal, (float3x3)instance.WorldInvTranspose));
    
    output.texcoord = input.texcoord;
    
    // ライト空間での位置を計算（シャドウマップ用）
    output.lightSpacePos = mul(worldPos, gShadowConstants.lightViewProj);
    
    output.instanceColor = instance.Color;
    
    return output;
}