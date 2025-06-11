#include "FullScreen.hlsli"

struct DepthBasedOutlineParams
{
    float4x4 projectionInverse;
    float outlineThickness;
};

ConstantBuffer<DepthBasedOutlineParams> gParams : register(b0);

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1); // シーンの深度テクスチャ
SamplerState gSampler : register(s0);

static const float kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

static const float kPreWittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    float uvStepSize = 1.0f / texSize.x;
    float2 difference = float2(0.0f, 0.0f);

    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            float2 texcoord = input.texCoord + kIndex3x3[x][y] * uvStepSize;
            float ndcDepth = gDepthTexture.Sample(gSampler, texcoord); // NDC座標系の深度値を取得
            float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gParams.projectionInverse);
            float viewZ = viewSpace.z * rcp(viewSpace.w); // 同次座標系からデカルト座標系に変換
            difference.x += viewZ * kPrewittHorizontalKernel[x][y];
            difference.y += viewZ * kPreWittVerticalKernel[x][y];
        }
    }

    float weight = length(difference);
    weight = saturate(weight * gParams.outlineThickness); // アウトラインの太さを調整

    PixelShaderOutput output;
    output.color.rgb = (1.0f - weight) * gTexture.Sample(gSampler, input.texCoord).rgb;
    output.color.a = 1.0f;

    return output.color;
}