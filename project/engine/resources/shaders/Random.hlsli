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

// =============================================================================
// PCG3D 整数ハッシュ
// =============================================================================
uint3 pcg3d(uint3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    return v;
}

// uint → [0, 1) float。上位 23bit を mantissa に詰めて [1,2) を作り 1 を引く
float uintToFloat01(uint u)
{
    return asfloat(0x3F800000u | (u >> 9u)) - 1.0f;
}

class RandomGenerator
{
    uint3 state;

    float3 Generate3d()
    {
        state = pcg3d(state);
        return float3(uintToFloat01(state.x),
                      uintToFloat01(state.y),
                      uintToFloat01(state.z));
    }

    float Generate1d()
    {
        state = pcg3d(state);
        return uintToFloat01(state.x);
    }
};