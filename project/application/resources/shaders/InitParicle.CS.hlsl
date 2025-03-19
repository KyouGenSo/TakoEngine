#include "Particle.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeCounter : register(u1);

[numthreads(1024, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle)0;

    }
    
    if (particleIndex == 0)
    {
        gFreeCounter[0] = 0;
    }

}