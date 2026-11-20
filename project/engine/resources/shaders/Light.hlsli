#ifndef TAKO_LIGHT_HLSLI
#define TAKO_LIGHT_HLSLI

// ライトの定数（Light.h の同じ名前の構造体と一致させること）
struct DirectionalLight
{
    float4 color;
    float3 direction;
    int lightType; // 0: Lambert、1: half-Lambert
    float intensity;
};

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay;
    int enable;
    int shadowSlice; // gShadowConstants.slices の番号（6 面の先頭）。負なら影なし
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float radius;
    float decay;
    float cosAngle;
    int enable;
    int shadowSlice; // gShadowConstants.slices の番号。負なら影なし
};

cbuffer LightConstants : register(b3)
{
    int gNumPointLights;
    int gNumSpotLights;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
StructuredBuffer<PointLight> gPointLights : register(t1);
StructuredBuffer<SpotLight> gSpotLight : register(t2);

// 距離による減衰（radius で 0 になる）
float PointLightAttenuation(PointLight light, float3 worldPos)
{
    float distance = length(light.position - worldPos);
    return pow(saturate(-distance / light.radius + 1.0), light.decay);
}

// 距離による減衰に、円錐の縁（cosAngle）へ向けての減衰を掛けた値
float SpotLightAttenuation(SpotLight light, float3 worldPos)
{
    float3 toSurface = worldPos - light.position;
    float distance = length(toSurface);
    float cosAngle = dot(toSurface / max(distance, 0.0001), light.direction);
    float cone = saturate((cosAngle - light.cosAngle) / (1.0 - light.cosAngle));
    return pow(saturate(-distance / light.radius + 1.0), light.decay) * cone;
}

#endif
