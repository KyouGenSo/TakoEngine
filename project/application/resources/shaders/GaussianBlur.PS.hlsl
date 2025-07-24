#include "FullScreen.hlsli"

static const float PI = 3.14159265f;

cbuffer Param : register(b0)
{
    float gSigma;
    int kernelSize;
    float2 direction; // x方向: float2(1,0), y方向: float2(0,1)
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float Gaussian(float x, float sigma)
{
    return 1.0f / (sqrt(2.0f * PI) * sigma) * exp(-(x * x) / (2.0f * sigma * sigma));
}

float4 GaussianBlur(float2 texcoord, float2 texSize, float2 dir)
{
    // 1ピクセルの長さ
    const float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y));
    
    float4 result = 0;
    
    float sum = 0.0f; // 重みの合計
    
    for (int karnelStep = -kernelSize / 2; karnelStep <= kernelSize / 2; ++karnelStep)
    {
        float2 uvOffset = texcoord;
        uvOffset.x += ((karnelStep + 0.5f) * texOffset.x) * dir.x;
        uvOffset.y += ((karnelStep + 0.5f) * texOffset.y) * dir.y;
        
        float weight = Gaussian(float(karnelStep), gSigma) + Gaussian(float(karnelStep + 1), gSigma);

        result.xyz += gTexture.Sample(gSampler, uvOffset).xyz * weight;
        
        sum += weight;
    }
    
    result *= (1.0f / sum); // normalizing the result
    
    return result; // return the blurred result
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    output.color.a = 1.0f;
    
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    float4 resultColor = GaussianBlur(input.texCoord, texSize, direction);

    //resultColor.rgb += output.color.rgb;
    
    return resultColor; // return the final color
}