#include "Particle.hlsli"

struct Particle
{
    float3 translate;
    float3 scale;
    float3 velocity;
    float4 color;
    float lifeTime;
    float currentTime;
};

struct PerView
{
    float4x4 viewProj;
    float4x4 billboardMat;
};

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    Particle particle = gParticles[instanceID];
    float4x4 worldMat = gPerView.billboardMat;
    
    worldMat[0] *= particle.scale.x;
    worldMat[1] *= particle.scale.y;
    worldMat[2] *= particle.scale.z;
    worldMat[3].xyz = particle.translate;
    
    output.pos = mul(input.pos, mul(worldMat, gPerView.viewProj));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    
    return output;
}