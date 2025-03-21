#include "Particle.hlsli"
#include "Random.hlsli"

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
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    RandomGenerator generator;
    generator.seed = (DTid + gPerFrame.time) * gPerFrame.time;
    
    if(gEmitter.isEmit != 0)
    {
        for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
        {
            int freeListIndex;
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
            
            if (0 <= freeListIndex && freeListIndex < kMaxParticles)
            {
                uint particleIndex = gFreeList[freeListIndex];
                gParticles[particleIndex].scale = float3(1.0f, 1.0f, 1.0f);
                gParticles[particleIndex].translate = gEmitter.center + generator.Generate3d() * gEmitter.radius;
                gParticles[particleIndex].velocity = generator.Generate3d();
                gParticles[particleIndex].lifeTime = 1.0f;
                gParticles[particleIndex].color.rgb = generator.Generate3d();
                gParticles[particleIndex].color.a = 1.0f;
            }
            else
            {
                InterlockedAdd(gFreeListIndex[0], 1);
                break;
            }
            
        }
    }
}