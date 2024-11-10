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
ConstantBuffer<PointLight> gPointLight : register(b3);
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
        
        //---------------------------------- Directional Light ----------------------------------
        float32_t3 directionalLightDiffuse;
        float32_t3 directionalLightSpecular;
        
        if (gDirectionalLight.lightType == 0)// Lambertian reflection
        {
            float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // îΩéÀã≠ìx
            
            // ägéUîΩéÀ
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            directionalLightDiffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
            
            // ãæñ îΩéÀ
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        }
        else if (gDirectionalLight.lightType == 1) // half-Lambertian reflection
        {
            float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // îΩéÀã≠ìx
            
            // ägéUîΩéÀ
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            directionalLightDiffuse = texColor.rgb * gMaterial.color.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos;
            
            // ãæñ îΩéÀ
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        }
        
        //---------------------------------- Point Light ----------------------------------
        float32_t3 pointLightDiffuse;
        float32_t3 pointLightSpecular;
        
        if(gPointLight.enable != 0)
        {
            float32_t3 pointLightDir = normalize(input.worldPos - gPointLight.position);
            
            float32_t3 halfVector = normalize(-pointLightDir + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess); // îΩéÀã≠ìx
            
            float32_t distance = length(gPointLight.position - input.worldPos);
            float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay); // ãóó£Ç…ÇÊÇÈå∏êä(0.0f ~ 1.0f
            float32_t3 pointLightColor = gPointLight.color.rgb * gPointLight.intensity * factor;
            
            // ägéUîΩéÀ
            float NdotL = saturate(dot(normalize(input.normal), -pointLightDir));
            pointLightDiffuse = texColor.rgb * gMaterial.color.rgb * pointLightColor * NdotL;
            
            // ãæñ îΩéÀ
            pointLightSpecular = gPointLight.color.rgb * gPointLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
            
        }else
        {
            pointLightDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
            pointLightSpecular = float32_t3(0.0f, 0.0f, 0.0f);
        }
        
        if (gMaterial.enableHighlight != 0)
        {
            output.color.rgb = directionalLightDiffuse + pointLightDiffuse + directionalLightSpecular + pointLightSpecular;

        }
        else
        {
            output.color.rgb = directionalLightDiffuse + pointLightDiffuse;
        }
        
        output.color.a = gMaterial.color.a * texColor.a;
    }
    else
    {
        output.color = texColor * gMaterial.color;
    }
    
    return output;
}