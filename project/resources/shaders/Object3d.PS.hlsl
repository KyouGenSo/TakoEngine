#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
    int32_t enableHighlight;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    int32_t lightType;
    float intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    int32_t enable;
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float32_t3 direction;
    float radius;
    float decay;
    float cosAngle;
    int32_t enable;
};

cbuffer LightConstants : register(b3)
{
    int gNumPointLights;
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
ConstantBuffer<SpotLight> gSpotLight : register(b4);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

StructuredBuffer<PointLight> gPointLights : register(t1);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float32_t4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    
    if (gMaterial.enableLighting != 0)
    {
        float32_t3 toEye = normalize(gCamera.worldPos - input.worldPos);
        
        //---------------------------------- Directional Light ----------------------------------
        float32_t3 directionalLightDiffuse;
        float32_t3 directionalLightSpecular;
        
        if (gDirectionalLight.lightType == 0)// Lambertian reflection
        {
            float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // ”½ŽË‹­“x
            
            // ŠgŽU”½ŽË
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            directionalLightDiffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
            
            // ‹¾–Ê”½ŽË
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        }
        else if (gDirectionalLight.lightType == 1) // half-Lambertian reflection
        {
            float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // ”½ŽË‹­“x
            
            // ŠgŽU”½ŽË
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            directionalLightDiffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos;
            
            // ‹¾–Ê”½ŽË
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        }
        
        //---------------------------------- Point Light ----------------------------------
        float32_t3 totalPointLightDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t3 totalPointLightSpecular = float32_t3(0.0f, 0.0f, 0.0f);

        for (int i = 0; i < gNumPointLights; i++)
        {
            if (gPointLights[i].enable != 0)
            {
                float32_t3 pointLightDir = normalize(input.worldPos - gPointLights[i].position);
        
                float32_t3 halfVector = normalize(-pointLightDir + toEye);
                float NdotH = dot(normalize(input.normal), halfVector);
                float specularPow = pow(saturate(NdotH), gMaterial.shininess);
        
                float32_t distance = length(gPointLights[i].position - input.worldPos);
                float32_t factor = pow(saturate(-distance / gPointLights[i].radius + 1.0f), gPointLights[i].decay);
                float32_t3 pointLightColor = gPointLights[i].color.rgb * gPointLights[i].intensity * factor;
        
                // ŠgŽU”½ŽË
                float NdotL = saturate(dot(normalize(input.normal), -pointLightDir));
                totalPointLightDiffuse += texColor.rgb * gMaterial.color.rgb * pointLightColor * NdotL;
        
                // ‹¾–Ê”½ŽË
                totalPointLightSpecular += gPointLights[i].color.rgb * gPointLights[i].intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
            }
        }
        
        //---------------------------------- Spot Light ----------------------------------
        float32_t3 spotLightDiffuse;
        float32_t3 spotLightSpecular;
        
        if (gSpotLight.enable != 0)
        {
            float32_t3 spotLightDirOnSurface = normalize(input.worldPos - gSpotLight.position);
            
            float32_t3 halfVector = normalize(-spotLightDirOnSurface + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // ”½ŽË‹­“x
            
            float32_t distance = length(gSpotLight.position - input.worldPos);
            float32_t factor = pow(saturate(-distance / gSpotLight.radius + 1.0f), gSpotLight.decay); // ‹——£‚É‚æ‚éŒ¸Š(0.0f ~ 1.0f
            
            float32_t cosAngle = dot(spotLightDirOnSurface, gSpotLight.direction);
            float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (1.0f - gSpotLight.cosAngle));
            
            float32_t3 spotLightColor = gSpotLight.color.rgb * gSpotLight.intensity * factor * falloffFactor;
            
            // ŠgŽU”½ŽË
            float NdotL = saturate(dot(normalize(input.normal), -spotLightDirOnSurface));
            spotLightDiffuse = texColor.rgb * gMaterial.color.rgb * spotLightColor * NdotL;
            
            // ‹¾–Ê”½ŽË
            spotLightSpecular = gSpotLight.color.rgb * gSpotLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        }
        else
        {
            spotLightDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
            spotLightSpecular = float32_t3(0.0f, 0.0f, 0.0f);
        }
        
        output.color.rgb = directionalLightDiffuse + totalPointLightDiffuse + spotLightDiffuse;
        
        if (gMaterial.enableHighlight != 0)
        {
            output.color.rgb += directionalLightSpecular + totalPointLightSpecular + spotLightSpecular;

        }
        
        output.color.a = gMaterial.color.a * texColor.a;
    }
    else
    {
        output.color = texColor * gMaterial.color;
    }
    
    return output;
}