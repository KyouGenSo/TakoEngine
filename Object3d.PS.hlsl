#include "Object3d.hlsli"

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
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float32_t4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    if (gMaterial.enableLighting != 0)
    {
        if(gDirectionalLight.lightType == 0)
        {
            // Lambertian reflection
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            output.color = texColor * gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * NdotL;
        }
        else if (gDirectionalLight.lightType == 1)
        {
            // half-Lambertian reflection
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            output.color = texColor * gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * cos;
        }
        
        // This is the original code
        //float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        //float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
        //output.color = texColor * gMaterial.color * gDirectionalLight.color * gDirectionalLight.intensity * cos;
    }
    else
    {
        output.color = texColor * gMaterial.color;
    }
    
    return output;
}