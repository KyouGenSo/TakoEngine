float3 rand3dTo3d(float3 value)
{
    float3 result;
    result.x = frac(sin(dot(value, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
    result.y = frac(sin(dot(value, float3(269.5f, 183.3f, 246.1f))) * 43758.5453f);
    result.z = frac(sin(dot(value, float3(419.2f, 371.9f, 124.6f))) * 43758.5453f);
    return result;
}

float rand3dTo1d(float3 value)
{
    return frac(sin(dot(value, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float rand2dTo1d(float2 value)
{
    return frac(sin(dot(value, float2(127.1f, 311.7f))) * 43758.5453f);
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