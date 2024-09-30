#include "NoTex.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    int32_t lightType;
    float intensity;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
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
        
        // This is the original code
        //float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        //float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
        //output.color = texColor * gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * cos;
    }
    else
    {
        output.color = gMaterial.color;
    }
    
    return output;
}