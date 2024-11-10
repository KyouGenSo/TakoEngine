#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    int32_t lightType;
    float intensity;
};

struct Camera
{
    float32_t3 worldPos;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float32_t4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    
    if (gMaterial.enableLighting != 0)
    {
        float32_t3 toEye = normalize(gCamera.worldPos - input.worldPos);
        float32_t3 reflectLight = reflect(gDirectionalLight.direction, input.normal);
        
        float RdotE = dot(reflectLight, toEye);
        float specularPow = pow(saturate(RdotE), gMaterial.shininess); // ”½ŽË‹­“x
        
        float32_t3 diffuse;
        float32_t3 specular;
        
        if (gDirectionalLight.lightType == 0)// Lambertian reflection
        {
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            // ŠgŽU”½ŽË
            diffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
            // ‹¾–Ê”½ŽË
            specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
            
            output.color.rgb = diffuse + specular;
            output.color.a = gMaterial.color.a * texColor.a;
        }
        else if (gDirectionalLight.lightType == 1) // half-Lambertian reflection
        {
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            // ŠgŽU”½ŽË
            diffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos;
            // ‹¾–Ê”½ŽË
            specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
            
            output.color.rgb = diffuse + specular;
            output.color.a = gMaterial.color.a * texColor.a;
        }
    }
    else
    {
        output.color = texColor * gMaterial.color;
    }
    
    return output;
}