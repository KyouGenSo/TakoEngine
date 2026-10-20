#include "FullScreen.hlsli"
#include "Common.hlsli"

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

ConstantBuffer<FogParam> gParam : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
Texture2D<float4> gTexture : register(t0); // シーンのカラーテクスチャ
Texture2D gDepthTexture : register(t1); // シーンの深度テクスチャ

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 FogColor(float2 texcoord)
{
    float4 sceneColor = gTexture.Sample(gPointWrap, texcoord);

    float depth = gDepthTexture.Sample(gPointWrap, texcoord).r;

    // 深度値をリニア化
    float linearDepth = gCamera.nearPlane * gCamera.farPlane / (gCamera.farPlane - depth * (gCamera.farPlane - gCamera.nearPlane));

    // 距離に対する指数減衰でフォグの濃さを計算
    float fogFactor = exp(-linearDepth * gParam.density);
    fogFactor = saturate(fogFactor);

    float4 finalColor = lerp(gParam.color, sceneColor, fogFactor);

    return finalColor;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gPointWrap, input.texCoord);
    output.color.a = 1.0f;
    
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    float4 fogColor = FogColor(input.texCoord);

    return fogColor;

}