#include "FullScreen.hlsli"

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
    const float2 texOffset = 1.0f / texSize;
    
    // Gaussian Kernel
    const float weight[5] = { 0.227027f, 0.19459465f, 0.1216216f, 0.054054f, 0.016216f };
    
    // 画像の明るい部分を抽出
    float4 bColor = BloomExtract(texcoord);
    
    float3 result;
    result.x = bColor.x * weight[0];
    result.y = bColor.y * weight[0];
    result.z = bColor.z * weight[0];
    
    //画像の明るい部分を横方向にぼかす
    for (int i = 1; i < 5; i++)
    {
        float2 weightOffset = float2(texOffset.x * i, 0.0f);
        result += BloomExtract(texcoord + weightOffset * gVignetteRedBloomParam.blurSize).xyz * weight[i];
        result += BloomExtract(texcoord - weightOffset * gVignetteRedBloomParam.blurSize).xyz * weight[i];
    }
    
    //画像の明るい部分を縦方向にぼかす
    for (int j = 1; j < 5; j++)
    {
        float2 weightOffset = float2(0.0f, texOffset.y * j);
        result += BloomExtract(texcoord + weightOffset * gVignetteRedBloomParam.blurSize).xyz * weight[j];
        result += BloomExtract(texcoord - weightOffset * gVignetteRedBloomParam.blurSize).xyz * weight[j];
    }
    
    return float4(result, 1.0f);
}

float4 BloomCombine(float2 texcoord, float2 texSize)
{
    //float4 baseColor = gTexture.Sample(gSampler, texcoord);
    //float4 blurColor = GaussianBlur(texcoord, texSize);
    
    float4 baseColor = AdjustSaturation(gTexture.Sample(gSampler, texcoord), 1.0f);
    float4 blurColor = AdjustSaturation(GaussianBlur(texcoord, texSize, float2(1.0f, 0.0f)) * GaussianBlur(texcoord, texSize, float2(0.0f, 1.0f)), 0.6f);
    
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
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    float4 bloomColor = BloomCombine(input.texCoord, texSize);
    
    float3 VignetteColor = VignetteRed(bloomColor, input.texCoord);
    
    return float4(VignetteColor, 1.0f);

}