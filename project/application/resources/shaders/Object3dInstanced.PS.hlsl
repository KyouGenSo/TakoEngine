#include "Object3dInstanced.hlsli"

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
ConstantBuffer<ShadowConstants> gShadowConstants : register(b4);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

StructuredBuffer<PointLight> gPointLights : register(t1);
StructuredBuffer<SpotLight> gSpotLight : register(t2);

TextureCube<float4> gEnvironmentMap : register(t3); // Optional, if environment mapping is used
Texture2D<float> gShadowMap : register(t4); // シャドウマップ
SamplerComparisonState gShadowSampler : register(s1); // シャドウマップ用比較サンプラー

// シャドウファクターを計算（動的PCF付き）
float CalculateShadowFactor(float4 lightSpacePos, float3 normal)
{
    // 透視変換の実行
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // NDC空間からテクスチャ座標に変換
    float2 shadowTexCoord;
    shadowTexCoord.x = projCoords.x * 0.5 + 0.5;
    shadowTexCoord.y = -projCoords.y * 0.5 + 0.5;
    
    // シャドウマップの範囲外チェック
    if (shadowTexCoord.x < 0.0 || shadowTexCoord.x > 1.0 ||
        shadowTexCoord.y < 0.0 || shadowTexCoord.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0; // シャドウマップの範囲外は照明
    }
    
    // 法線オフセットバイアスを適用
    float3 lightDir = normalize(gDirectionalLight.direction);
    float NdotL = max(0.0, dot(normal, -lightDir));
    float bias = gShadowConstants.shadowBias + (1.0 - NdotL) * gShadowConstants.normalOffsetBias;
    
    // バイアスを適用
    float currentDepth = projCoords.z - bias;
    
    // 動的PCF（Percentage Closer Filtering）
    float shadowFactor = 0.0;
    float2 texelSize = 1.0 / gShadowConstants.shadowMapSize;
    
    // カーネルサイズに基づくサンプリング範囲
    int kernelRadius = int(gShadowConstants.pcfKernelSize) / 2;
    float sampleCount = 0.0;
    
    for (int x = -kernelRadius; x <= kernelRadius; ++x) {
        for (int y = -kernelRadius; y <= kernelRadius; ++y) {
            float2 offset = float2(x, y) * texelSize;
            shadowFactor += gShadowMap.SampleCmpLevelZero(gShadowSampler, 
                           shadowTexCoord + offset, currentDepth);
            sampleCount += 1.0;
        }
    }
    shadowFactor /= sampleCount; // サンプル数で平均
    
    return shadowFactor;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // シャドウファクターを計算
    float shadowFactor = 1.0; // デフォルトでは影なし
    if (gShadowConstants.enableShadow != 0) {
        shadowFactor = CalculateShadowFactor(input.lightSpacePos, normalize(input.normal));
    }
    
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
        
                float distance = length(gPointLights[i].position - input.worldPos);
                float factor = pow(saturate(-distance / gPointLights[i].radius + 1.0f), gPointLights[i].decay);
                float3 pointLightColor = gPointLights[i].color.rgb * gPointLights[i].intensity * factor;
        
                // 拡散反射（インスタンスカラーを使用）
                float NdotL = saturate(dot(normalize(input.normal), -pointLightDir));
                totalPointLightDiffuse += texColor.rgb * input.instanceColor.rgb * pointLightColor * NdotL;
        
                // 鏡面反射
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
                float specularPow = pow(saturate(NdotH), gMaterial.shininess);
                
                float distance = length(gSpotLight[j].position - input.worldPos);
                float factor = pow(saturate(-distance / gSpotLight[j].radius + 1.0f), gSpotLight[j].decay);
                
                float cosAngle = dot(spotLightDirOnSurface, gSpotLight[j].direction);
                float falloffFactor = saturate((cosAngle - gSpotLight[j].cosAngle) / (1.0f - gSpotLight[j].cosAngle));
                
                float3 spotLightColor = gSpotLight[j].color.rgb * gSpotLight[j].intensity * factor * falloffFactor;
                
                // 拡散反射（インスタンスカラーを使用）
                float NdotL = saturate(dot(normalize(input.normal), -spotLightDirOnSurface));
                spotLightDiffuse += texColor.rgb * input.instanceColor.rgb * spotLightColor * NdotL;
                
                // 鏡面反射
                spotLightSpecular += gSpotLight[j].color.rgb * gSpotLight[j].intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
            }
        }
        
        //-----------------------------------------------------------------------------------------------------------------------------
        
        output.color.rgb = directionalLightDiffuse + totalPointLightDiffuse + spotLightDiffuse;
        
        if (gMaterial.enableHighlight != 0)
        {
            output.color.rgb += directionalLightSpecular + totalPointLightSpecular + spotLightSpecular;

        }
        
        // アルファ値もインスタンスカラーを使用
        output.color.a = input.instanceColor.a * texColor.a;

        if (gMaterial.enableEnvMap != 0)
        {
        // 環境マッピングを適用
            float3 cameraToPos = normalize(input.worldPos - gCamera.worldPos);
            float3 refelectedVector = reflect(cameraToPos, normalize(input.normal));
            float4 environmentColor = gEnvironmentMap.Sample(gSampler, refelectedVector);

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
            float3 cameraToPos = normalize(input.worldPos - gCamera.worldPos);
            float3 refelectedVector = reflect(cameraToPos, normalize(input.normal));
            float4 environmentColor = gEnvironmentMap.Sample(gSampler, refelectedVector);

            output.color.rgb += environmentColor.rgb * gMaterial.envMapCoefficient;
        }
    }
    
    return output;
}