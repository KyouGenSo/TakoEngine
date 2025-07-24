#include "FullScreen.hlsli"

static const float PI = 3.14159265f;

struct BloomParam
{
    float intensity;
    float threshold;
    float sigma;
    int kernelSize;
    float2 direction; // x方向: float2(1,0), y方向: float2(0,1)
};

ConstantBuffer<BloomParam> gBloomParam : register(b0);
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
    //// 閾値の範囲を定義
    //float minThreshold = gBloomParam.threshold;
    //float maxThreshold = gBloomParam.threshold + 0.1f;
    //// smoothstepで滑らかな閾値適用
    //float brightness = max(color.r, max(color.g, color.b));
    //float factor = smoothstep(minThreshold, maxThreshold, brightness);
    //return color * factor;

    float brightness = dot(color.rgb, float3(0.299, 0.587, 0.114));

    float4 output = brightness > gBloomParam.threshold ? color : float4(0.0f, 0.0f, 0.0f, 0.0f);

    return output;
}

float4 GaussianBlur(float2 texcoord, float2 texSize, float2 dir)
{
    // 1ピクセルの長さ
    float2 uvOffset;
    
    // 1ピクセルの長さ
    const float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y));
    
    float4 result = BloomExtract(texcoord);
    
    float sum = 0.0f; // 重みの合計
    
    float weight; // 重み
    
    for (int karnelStep = -gBloomParam.kernelSize / 2; karnelStep <= gBloomParam.kernelSize / 2; ++karnelStep)
    {
        if (karnelStep == 0)
        {
            continue; // 中心のサンプルは既にresultに含まれているのでスキップ
        }

        uvOffset = texcoord;
        uvOffset.x += karnelStep * texOffset.x * dir.x;
        uvOffset.y += karnelStep * texOffset.y * dir.y;
        
        weight = Gaussian(float(karnelStep), gBloomParam.sigma);

        result.xyz += BloomExtract(uvOffset).xyz * weight;
        
        sum += weight;
        
    }
    
    result *= (1.0f / sum); // normalizing the result
    
    return result; // return the blurred result
}

float4 SquareGaussianBlur(float2 texcoord, float2 texSize)
{
    // 9x9の四角形カーネルのオフセット
    const float2 offsets9x9[81] =
    {
        float2(-4.0, -4.0), float2(-3.0, -4.0), float2(-2.0, -4.0), float2(-1.0, -4.0), float2(0.0, -4.0), float2(1.0, -4.0), float2(2.0, -4.0), float2(3.0, -4.0), float2(4.0, -4.0),
        float2(-4.0, -3.0), float2(-3.0, -3.0), float2(-2.0, -3.0), float2(-1.0, -3.0), float2(0.0, -3.0), float2(1.0, -3.0), float2(2.0, -3.0), float2(3.0, -3.0), float2(4.0, -3.0),
        float2(-4.0, -2.0), float2(-3.0, -2.0), float2(-2.0, -2.0), float2(-1.0, -2.0), float2(0.0, -2.0), float2(1.0, -2.0), float2(2.0, -2.0), float2(3.0, -2.0), float2(4.0, -2.0),
        float2(-4.0, -1.0), float2(-3.0, -1.0), float2(-2.0, -1.0), float2(-1.0, -1.0), float2(0.0, -1.0), float2(1.0, -1.0), float2(2.0, -1.0), float2(3.0, -1.0), float2(4.0, -1.0),
        float2(-4.0, 0.0), float2(-3.0, 0.0), float2(-2.0, 0.0), float2(-1.0, 0.0), float2(0.0, 0.0), float2(1.0, 0.0), float2(2.0, 0.0), float2(3.0, 0.0), float2(4.0, 0.0),
        float2(-4.0, 1.0), float2(-3.0, 1.0), float2(-2.0, 1.0), float2(-1.0, 1.0), float2(0.0, 1.0), float2(1.0, 1.0), float2(2.0, 1.0), float2(3.0, 1.0), float2(4.0, 1.0),
        float2(-4.0, 2.0), float2(-3.0, 2.0), float2(-2.0, 2.0), float2(-1.0, 2.0), float2(0.0, 2.0), float2(1.0, 2.0), float2(2.0, 2.0), float2(3.0, 2.0), float2(4.0, 2.0),
        float2(-4.0, 3.0), float2(-3.0, 3.0), float2(-2.0, 3.0), float2(-1.0, 3.0), float2(0.0, 3.0), float2(1.0, 3.0), float2(2.0, 3.0), float2(3.0, 3.0), float2(4.0, 3.0),
        float2(-4.0, 4.0), float2(-3.0, 4.0), float2(-2.0, 4.0), float2(-1.0, 4.0), float2(0.0, 4.0), float2(1.0, 4.0), float2(2.0, 4.0), float2(3.0, 4.0), float2(4.0, 4.0)
    };

    // 5x5の四角形カーネルのオフセット
    const float2 offsets5x5[25] =
    {
        float2(-2.0, -2.0), float2(-1.0, -2.0), float2(0.0, -2.0), float2(1.0, -2.0), float2(2.0, -2.0),
        float2(-2.0, -1.0), float2(-1.0, -1.0), float2(0.0, -1.0), float2(1.0, -1.0), float2(2.0, -1.0),
        float2(-2.0, 0.0), float2(-1.0, 0.0), float2(0.0, 0.0), float2(1.0, 0.0), float2(2.0, 0.0),
        float2(-2.0, 1.0), float2(-1.0, 1.0), float2(0.0, 1.0), float2(1.0, 1.0), float2(2.0, 1.0),
        float2(-2.0, 2.0), float2(-1.0, 2.0), float2(0.0, 2.0), float2(1.0, 2.0), float2(2.0, 2.0)
    };

    float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y)); // 1ピクセルの長さ
    float4 result = float4(0.0, 0.0, 0.0, 0.0); // 結果の初期化
    float sum = 0.0f; // 重みの合計

    for (int i = 0; i < 25; i++)
    {
        // サンプル位置の座標
        float2 sampleCoord = texcoord + offsets5x5[i] * texOffset * 1;

        // ガウシアン重み（中心からの距離に基づく）
        float weight = Gaussian(length(offsets5x5[i]), gBloomParam.sigma);

        // サンプルと重みを加算
        result.xyz += BloomExtract(sampleCoord).xyz * weight;
        sum += weight;
    }

    // 正規化
    result *= (1.0f / sum);
    
    return result;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    output.color.a = 1.0f;
    
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    float4 bloomColor = GaussianBlur(input.texCoord, texSize, gBloomParam.direction);
    
    //float4 bloomColor = SquareGaussianBlur(input.texCoord, texSize);
    
    bloomColor.rgb *= gBloomParam.intensity;
    
    bloomColor.rgb += output.color.rgb;
    
    return bloomColor; // return the final combined color
}