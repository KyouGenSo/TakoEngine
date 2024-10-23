#include "FullScreen.hlsli"

static const float PI = 3.14159265f;

static const int KERNEL_SIZE = 30;

struct VignetteRedBloomParam
{
    float power;
    float threshold;
    float2 blurSize;
};

ConstantBuffer<VignetteRedBloomParam> gVignetteRedBloomParam : register(b0);
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

float4 BloomExtract(float2 texcoord)
{
    float4 color = gTexture.Sample(gSampler, texcoord);
    return saturate((color - gVignetteRedBloomParam.threshold) / (1.0f - gVignetteRedBloomParam.threshold));
}

float4 AdjustSaturation(float4 color, float saturation)
{
    float gray = dot(color.rgb, float3(0.3f, 0.59f, 0.11f));
    return lerp(gray, color, saturation);
}

float4 GaussianBlur(float2 texcoord, float2 texSize, float2 dir)
{
    // 1ピクセルの長さ
    float2 uvOffset;
    
    // 1ピクセルの長さ
    const float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y));
    
    float4 result = BloomExtract(texcoord);
    //float4 result = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    float sum;
    
    float weight;
    
    for (int karnelStep = -KERNEL_SIZE / 2; karnelStep <= KERNEL_SIZE / 2; ++karnelStep)
    {
        uvOffset = texcoord;
        uvOffset.x += karnelStep * texOffset.x * dir.x;
        uvOffset.y += karnelStep * texOffset.y * dir.y;
        
        weight = Gaussian(karnelStep, 2.0f);
        
        //result.xyz += BloomExtract(uvOffset).xyz * weight;
        result.xyz += gTexture.Sample(gSampler, uvOffset).xyz * weight;
        
        sum += weight;
        
    }
    
    result *= (1.0f / sum);
    
    return result;
}

float4 BloomCombine(float2 texcoord, float2 texSize)
{
    //float4 baseColor = AdjustSaturation(gTexture.Sample(gSampler, texcoord), 1.0f);
    //float4 blurColor = AdjustSaturation(GaussianBlur(texcoord, texSize, float2(1.0f, 0.0f)) * GaussianBlur(texcoord, texSize, float2(0.0f, 1.0f)), 0.6f);
    
    float4 baseColor = gTexture.Sample(gSampler, texcoord);
    float4 blurColor = GaussianBlur(texcoord, texSize, float2(1.0f, 0.0f)) * GaussianBlur(texcoord, texSize, float2(0.0f, 1.0f));
    
    return baseColor + blurColor;
}

float3 VignetteRed(float4 color, float2 texCoord)
{
    float2 correct = texCoord * (1.0f - texCoord.xy);
    
    float vignette = correct.x * correct.y * 15.0f;
    
    vignette = saturate(pow(vignette, gVignetteRedBloomParam.power));
    
    // 暗くなる部分に赤色を追加
    float3 redTint = float3(1.0f, 0.0f, 0.0f); // 赤色
    return lerp(color.rgb, redTint, 1.0f - vignette); // vignetteで色を補間
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    output.color.a = 1.0f;
    
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);

    //float4 bloomColor = BloomCombine(input.texCoord, texSize);
    
    float4 bloomColor = GaussianBlur(input.texCoord, texSize, float2(1.0f, 0.0f)) * GaussianBlur(input.texCoord, texSize, float2(0.0f, 1.0f));
    
    //output.color.rgb += bloomColor.rgb;
    
    float3 VignetteColor = VignetteRed(bloomColor, input.texCoord);
    //float3 VignetteColor = VignetteRed(output.color, input.texCoord);
    
    //output.color.rgb += VignetteColor;

    bloomColor.rgb += VignetteColor;
    
    //return float4(VignetteColor, 1.0f);
    return bloomColor;
    //return output.color;

}