#include "Object3dInstanced.hlsli"
#include "View.hlsli"
#include "Common.hlsli"
#include "Light.hlsli"
#include "Shadow.hlsli"

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

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<View> gView : register(b2);

Texture2D<float4> gTexture : register(t0);

TextureCube<float4> gEnvironmentMap : register(t3);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 texColor = gTexture.Sample(gLinearWrap, transformedUV.xy);
    
    float shadowFactor = DirectionalShadow(input.worldPos, input.normal, gDirectionalLight.direction);
    
    if (gMaterial.enableLighting != 0)
    {
        float3 toEye = normalize(gView.worldPos - input.worldPos);
        
        //---------------------------------- Directional Light ----------------------------------
        float3 directionalLightDiffuse;
        float3 directionalLightSpecular;
        
        if (gDirectionalLight.lightType == 0)// Lambertian reflection
        {
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess);
            
            // 拡散反射（インスタンスカラーを使用）
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            directionalLightDiffuse = texColor.rgb * input.instanceColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL * shadowFactor;
            
            // 鏡面反射
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f) * shadowFactor;
        }
        else if (gDirectionalLight.lightType == 1) // half-Lambertian reflection
        {
            float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
            float NdotH = dot(normalize(input.normal), halfVector);
            float specularPow = pow(saturate(NdotH), gMaterial.shininess);
            
            // 拡散反射（インスタンスカラーを使用）
            float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
            float cos = pow(NdotL * 0.5 + 0.5, 2.0f);
            directionalLightDiffuse = texColor.rgb * input.instanceColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos * shadowFactor;
            
            // 鏡面反射
            directionalLightSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f) * shadowFactor;
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
        
                float factor = PointLightAttenuation(gPointLights[i], input.worldPos);
                float pointShadow = 1.0;
                if (factor > 0.0)
                {
                    pointShadow = PointShadow(gPointLights[i].shadowSlice, gPointLights[i].position, input.worldPos, input.normal);
                }
                float3 pointLightColor = gPointLights[i].color.rgb * gPointLights[i].intensity * factor * pointShadow;
        
                // 拡散反射（インスタンスカラーを使用）
                float NdotL = saturate(dot(normalize(input.normal), -pointLightDir));
                totalPointLightDiffuse += texColor.rgb * input.instanceColor.rgb * pointLightColor * NdotL;
        
                // 鏡面反射
                totalPointLightSpecular += gPointLights[i].color.rgb * gPointLights[i].intensity * specularPow * float3(1.0f, 1.0f, 1.0f) * pointShadow;
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
                float specularPow = pow(saturate(NdotH), gMaterial.shininess);
                
                float factor = SpotLightAttenuation(gSpotLight[j], input.worldPos);
                float spotShadow = 1.0;
                if (factor > 0.0)
                {
                    spotShadow = SpotShadow(gSpotLight[j].shadowSlice, gSpotLight[j].position, input.worldPos, input.normal);
                }
                float3 spotLightColor = gSpotLight[j].color.rgb * gSpotLight[j].intensity * factor * spotShadow;
                
                // 拡散反射（インスタンスカラーを使用）
                float NdotL = saturate(dot(normalize(input.normal), -spotLightDirOnSurface));
                spotLightDiffuse += texColor.rgb * input.instanceColor.rgb * spotLightColor * NdotL;
                
                // 鏡面反射
                spotLightSpecular += gSpotLight[j].color.rgb * gSpotLight[j].intensity * specularPow * float3(1.0f, 1.0f, 1.0f) * spotShadow;
            }
        }
        
        //-----------------------------------------------------------------------------------------------------------------------------
        
        output.color.rgb = directionalLightDiffuse + totalPointLightDiffuse + spotLightDiffuse;
        
        if (gMaterial.enableHighlight != 0)
        {
            output.color.rgb += directionalLightSpecular + totalPointLightSpecular + spotLightSpecular;

        }
        output.color.rgb *= ShadowCascadeDebugTint(input.worldPos);
        
        // アルファ値もインスタンスカラーを使用
        output.color.a = input.instanceColor.a * texColor.a;

        if (gMaterial.enableEnvMap != 0)
        {
        // 環境マッピングを適用
            float3 cameraToPos = normalize(input.worldPos - gView.worldPos);
            float3 refelectedVector = reflect(cameraToPos, normalize(input.normal));
            float4 environmentColor = gEnvironmentMap.Sample(gLinearWrap, refelectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.envMapCoefficient;
        }
    }
    else
    {
        // ライティングなしの場合もインスタンスカラーを使用
        output.color = texColor * input.instanceColor;
        if (gMaterial.enableEnvMap != 0)
        {
        // 環境マッピングを適用
            float3 cameraToPos = normalize(input.worldPos - gView.worldPos);
            float3 refelectedVector = reflect(cameraToPos, normalize(input.normal));
            float4 environmentColor = gEnvironmentMap.Sample(gLinearWrap, refelectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.envMapCoefficient;
        }
    }
    
    return output;
}