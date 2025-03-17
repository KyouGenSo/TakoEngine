static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;
    float3 scale;
    float3 velocity;
    float4 color;
    float lifeTime;
    float currentTime;
};

RWStructuredBuffer<Particle> gParticles : register(u0);


[numthreads(1024, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        gParticles[particleIndex] = (Particle)0;
    }
}