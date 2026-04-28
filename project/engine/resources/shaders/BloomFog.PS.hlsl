#include "FullScreen.hlsli"

static const float PI = 3.14159265f;

static const int KERNEL_SIZE = 30;

struct BloomParam
{
    float intensity;
    float threshold;
    float sigma;
};

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

ConstantBuffer<BloomParam> gBloomParam : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<FogParam> gFogParam : register(b2);
Texture2D<float4> gTexture : register(t0);            // シーンのカラーテクスチャ
Texture2D gDepthTexture : register(t1);               // シーンの深度テクスチャ
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
    // 閾値の範囲を定義
    float minThreshold = gBloomParam.threshold - 0.1f;
    float maxThreshold = gBloomParam.threshold;
    // smoothstepで滑らかな閾値適用
    float brightness = max(color.r, max(color.g, color.b));
    float factor = smoothstep(minThreshold, maxThreshold, brightness);
    return color * factor;
}

float4 SquareGaussianBlur(float2 texcoord, float2 texSize)
{
    // 9x9の四角形カーネルのオフセット
    const float2 offsets[81] =
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

    float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y)); // 1ピクセルの長さ
    float4 result = float4(0.0, 0.0, 0.0, 0.0); // 結果の初期化
    float sum = 0.0f; // 重みの合計

    for (int i = 0; i < 81; i++)
    {
        // サンプル位置の座標
        float2 sampleCoord = texcoord + offsets[i] * texOffset * 1;

        // ガウシアン重み（中心からの距離に基づく）
        float weight = Gaussian(length(offsets[i]), gBloomParam.sigma);

        // サンプルと重みを加算
        result.xyz += BloomExtract(sampleCoord).xyz * weight;
        sum += weight;
    }

    // 正規化
    result *= (1.0f / sum);
    
    return result;
}

float4 FogColor(float2 texcoord)
{
    // シーンカラーを取得
    float4 sceneColor = gTexture.Sample(gSampler, texcoord);

    // 深度バッファからの深度値を取得
    float depth = gDepthTexture.Sample(gSampler, texcoord).r;

    // 深度値をリニア化
    float linearDepth = gCamera.nearPlane * gCamera.farPlane / (gCamera.farPlane - depth * (gCamera.farPlane - gCamera.nearPlane));

    // 体積フォグのファクターを計算
    float fogFactor = exp(-linearDepth * gFogParam.density);
    fogFactor = saturate(fogFactor); // [0, 1]の範囲にクランプ

    // シーンのカラーとフォグの色を補間
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
    
    float4 bloomColor = SquareGaussianBlur(input.texCoord, texSize);
    
    bloomColor.rgb *= gBloomParam.intensity;
    
    float4 fogColor = FogColor(input.texCoord);
    
    bloomColor.rgb += fogColor.rgb;
    
    
    return bloomColor;

}