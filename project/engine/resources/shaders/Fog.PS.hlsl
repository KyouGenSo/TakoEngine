#include "FullScreen.hlsli"

struct Camera
{
    float nearPlane;
    float farPlane;
};

struct FogParam
{
    float4 color;
    float density;
};

ConstantBuffer<FogParam> gFogParam : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
Texture2D<float4> gTexture : register(t0); // シーンのカラーテクスチャ
Texture2D gDepthTexture : register(t1); // シーンの深度テクスチャ
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 FogColor(float2 texcoord)
{
    float4 sceneColor = gTexture.Sample(gSampler, texcoord);

    float depth = gDepthTexture.Sample(gSampler, texcoord).r;

    // 深度値をリニア化
    float linearDepth = gCamera.nearPlane * gCamera.farPlane / (gCamera.farPlane - depth * (gCamera.farPlane - gCamera.nearPlane));

    // 距離に対する指数減衰でフォグの濃さを計算
    float fogFactor = exp(-linearDepth * gFogParam.density);
    fogFactor = saturate(fogFactor);

    float4 finalColor = lerp(gFogParam.color, sceneColor, fogFactor);

    return finalColor;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    output.color.a = 1.0f;
    
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    float4 fogColor = FogColor(input.texCoord);

    return fogColor;

}