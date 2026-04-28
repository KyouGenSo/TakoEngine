#include "NoTex.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    int lightType;
    float intensity;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    if (gMaterial.enableLighting != 0)
    {
        if (gDirectionalLight.lightType == 0)
        {
            // Lambertian reflection
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            output.color = gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * NdotL;
        }
        else if (gDirectionalLight.lightType == 1)
        {
            // half-Lambertian reflection
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            output.color = gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * cos;
        }
    }
    else
    {
        output.color = gMaterial.color;
    }
    
    return output;
}