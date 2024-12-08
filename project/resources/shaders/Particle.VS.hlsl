#include "Particle.hlsli"

struct ParticleForGPU
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

StructuredBuffer<ParticleForGPU> gParticleDatas : register(t0);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    ParticleForGPU particleData = gParticleDatas[instanceID];
    output.pos = mul(input.pos, particleData.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) particleData.World));
    output.color = particleData.color;
    return output;
}