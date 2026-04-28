#include "FullScreen.hlsli"

struct DepthBasedOutlineParams
{
    float outlineThickness; // Thickness of the outline
};

ConstantBuffer<DepthBasedOutlineParams> gParams : register(b0);

Texture2D<float4> gTexture : register(t0);
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

float GetLuminance(float3 color)
{
    return dot(color, float3(0.2125f, 0.7154f, 0.0721f));
}

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
            float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
            float luminance = GetLuminance(fetchColor);
            difference.x += luminance * kPrewittHorizontalKernel[x][y];
            difference.y += luminance * kPreWittVerticalKernel[x][y];
        }
    }

    float weight = length(difference);
    weight = saturate(weight * gParams.outlineThickness); // Adjust the weight to control the outline thickness

    PixelShaderOutput output;
    output.color.rgb = (1.0f - weight) * gTexture.Sample(gSampler, input.texCoord).rgb;
    output.color.a = 1.0f;

    return output.color;
}