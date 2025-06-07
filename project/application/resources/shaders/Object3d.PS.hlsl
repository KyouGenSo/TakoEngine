#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
    float shininess;
    float envMapCoefficient;
    int enableHighlight;
    int enableEnvMap;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    int lightType;
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
};

cbuffer LightConstants : register(b3)
{
    int gNumPointLights;
    int gNumSpotLights;
};

struct Camera
{
    float3 worldPos;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

StructuredBuffer<PointLight> gPointLights : register(t1);
StructuredBuffer<SpotLight> gSpotLight : register(t2);

TextureCube<float4> gEnvironmentMap : register(t3); // Optional, if environment mapping is used

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    if (gMaterial.enableLighting != 0)
    {
        float3 toEye = normalize(gCamera.worldPos - input.worldPos);
        
        //---------------------------------- Directional Light ----------------------------------
        float3 directionalLightDiffuse;
        float3 directionalLightSpecular;
        
        if (gDirectionalLight.lightType == 0)// Lambertian reflection
        {
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // ”½ŽË‹­“x
            
            // ŠgŽU”½ŽË
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            directionalLightDiffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
            
            // ‹¾–Ê”½ŽË
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
        }
        else if (gDirectionalLight.lightType == 1) // half-Lambertian reflection
        {
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // ”½ŽË‹­“x
            
            // ŠgŽU”½ŽË
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            directionalLightDiffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos;
            
            // ‹¾–Ê”½ŽË
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
        }
        
        //---------------------------------- Point Light ----------------------------------
        float3 totalPointLightDiffuse = float3(0.0f, 0.0f, 0.0f);
        float3 totalPointLightSpecular = float3(0.0f, 0.0f, 0.0f);

        for (int i = 0; i < gNumPointLights; i++)
        {
            if (gPointLights[i].enable != 0)
            {
                float3 pointLightDir = normalize(input.worldPos - gPointLights[i].position);
        
                float3 halfVector = normalize(-pointLightDir + toEye);
                float NdotH = dot(normalize(input.normal), halfVector);
                float specularPow = pow(saturate(NdotH), gMaterial.shininess);
        
                float distance = length(gPointLights[i].position - input.worldPos);
                float factor = pow(saturate(-distance / gPointLights[i].radius + 1.0f), gPointLights[i].decay);
                float3 pointLightColor = gPointLights[i].color.rgb * gPointLights[i].intensity * factor;
        
                // ŠgŽU”½ŽË
                float NdotL = saturate(dot(normalize(input.normal), -pointLightDir));
                totalPointLightDiffuse += texColor.rgb * gMaterial.color.rgb * pointLightColor * NdotL;
        
                // ‹¾–Ê”½ŽË
                totalPointLightSpecular += gPointLights[i].color.rgb * gPointLights[i].intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
            }
        }
        
        //---------------------------------- Spot Light ----------------------------------
        float3 spotLightDiffuse = float3(0.0f, 0.0f, 0.0f);
        float3 spotLightSpecular = float3(0.0f, 0.0f, 0.0f);
        
        for (int j = 0; j < gNumSpotLights; j++)
        {
            if (gSpotLight[j].enable != 0)
            {
                float3 spotLightDirOnSurface = normalize(input.worldPos - gSpotLight[j].position);
                
                float3 halfVector = normalize(-spotLightDirOnSurface + toEye);
                float NdotH = dot(normalize(input.normal), halfVector);
                float specularPow = pow(saturate(NdotH), gMaterial.shininess); // ”½ŽË‹­“x
                
                float distance = length(gSpotLight[j].position - input.worldPos);
                float factor = pow(saturate(-distance / gSpotLight[j].radius + 1.0f), gSpotLight[j].decay); // ‹——£‚É‚æ‚éŒ¸Š(0.0f ~ 1.0f
                
                float cosAngle = dot(spotLightDirOnSurface, gSpotLight[j].direction);
                float falloffFactor = saturate((cosAngle - gSpotLight[j].cosAngle) / (1.0f - gSpotLight[j].cosAngle));
                
                float3 spotLightColor = gSpotLight[j].color.rgb * gSpotLight[j].intensity * factor * falloffFactor;
                
                // ŠgŽU”½ŽË
                float NdotL = saturate(dot(normalize(input.normal), -spotLightDirOnSurface));
                spotLightDiffuse += texColor.rgb * gMaterial.color.rgb * spotLightColor * NdotL;
                
                // ‹¾–Ê”½ŽË
                spotLightSpecular += gSpotLight[j].color.rgb * gSpotLight[j].intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
            }
        }
        
        //-----------------------------------------------------------------------------------------------------------------------------
        
        output.color.rgb = directionalLightDiffuse + totalPointLightDiffuse + spotLightDiffuse;
        
        if (gMaterial.enableHighlight != 0)
        {
            output.color.rgb += directionalLightSpecular + totalPointLightSpecular + spotLightSpecular;

        }
        
        output.color.a = gMaterial.color.a * texColor.a;

        if (gMaterial.enableEnvMap != 0)
        {
        // ŠÂ‹«ƒ}ƒbƒsƒ“ƒO‚ð“K—p
            float3 cameraToPos = normalize(input.worldPos - gCamera.worldPos);
            float3 refelectedVector = reflect(cameraToPos, normalize(input.normal));
            float4 environmentColor = gEnvironmentMap.Sample(gSampler, refelectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.envMapCoefficient;
        }
    }
    else
    {
        output.color = texColor * gMaterial.color;
        if (gMaterial.enableEnvMap != 0)
        {
        // ŠÂ‹«ƒ}ƒbƒsƒ“ƒO‚ð“K—p
            float3 cameraToPos = normalize(input.worldPos - gCamera.worldPos);
            float3 refelectedVector = reflect(cameraToPos, normalize(input.normal));
            float4 environmentColor = gEnvironmentMap.Sample(gSampler, refelectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.envMapCoefficient;
        }
    }
    
    return output;
}