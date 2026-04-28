#include "Particle.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint particleIndex = DTid.x;
    
    if (particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle)0;
        gParticles[particleIndex].startColor.a = 0.0f;
        gParticles[particleIndex].endColor.a = 0.0f;
        
        gFreeList[particleIndex] = particleIndex;
    }
    
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = kMaxParticles - 1;
    }

}