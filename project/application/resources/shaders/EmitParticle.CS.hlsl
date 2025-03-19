#include "Particle.hlsli"

struct EmitterSphere
{
    float3 center;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint isEmit;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeCounter : register(u1);

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

float3 rand3dTo3d(float3 value)
{
    float3 result;
    // 0-1 random number generator
    result.x = frac(sin(dot(value, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    result.y = frac(sin(dot(value, float3(269.5f, 183.3f, 246.1f))) * 43758.5453f);
    result.z = frac(sin(dot(value, float3(419.2f, 371.9f, 124.6f))) * 43758.5453f);
    return result;
}

float rand3dTo1d(float3 value)
{
    return frac(sin(dot(value, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

class RandomGenerator
{
    float3 seed;
    
    float3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }
    
    float Generate1d()
    {
        float result = rand3dTo1d(seed);
        seed.x = result;
        return seed.x;
    }
};

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    RandomGenerator generator;
    generator.seed = (DTid + gPerFrame.time) * gPerFrame.time;
    
    if(gEmitter.isEmit != 0)
    {
        for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
        {
            int particleIndex;
            InterlockedAdd(gFreeCounter[0], 1, particleIndex);
            
            if (particleIndex < kMaxParticles)
            {
                gParticles[particleIndex].scale = float3(1.0f, 1.0f, 1.0f);
                gParticles[particleIndex].translate = generator.Generate3d() * 5.0f - 2.5f;
                gParticles[particleIndex].color.rgb = generator.Generate3d();
                gParticles[particleIndex].color.a = 1.0f;
            }
        }
    }
}