static const uint kMaxParticles = 1024;

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct Particle
{
    float3 translate;
    float3 scale;
    float3 velocity;
    float4 color;
    float lifeTime;
    float currentTime;
};