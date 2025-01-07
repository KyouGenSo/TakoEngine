#include "FullScreen.hlsli"

static const float PI = 3.14159265f;

static const int KERNEL_SIZE = 30;

struct VignetteRedBloomParam
{
    float power;
    float range;
    float intensity;
    float threshold;
    float sigma;
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
    // 臒l�͈̔͂��`
    float minThreshold = gVignetteRedBloomParam.threshold - 0.1f;
    float maxThreshold = gVignetteRedBloomParam.threshold;
    // smoothstep�Ŋ��炩��臒l�K�p
    float brightness = max(color.r, max(color.g, color.b));
    float factor = smoothstep(minThreshold, maxThreshold, brightness);
    return color * factor;
}

float4 GaussianBlur(float2 texcoord, float2 texSize, float2 dir)
{
    // 1�s�N�Z���̒���
    float2 uvOffset;
    
    // 1�s�N�Z���̒���
    const float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y));
    
    float4 result = BloomExtract(texcoord);
    
    float sum;
    
    float weight;
    
    for (int karnelStep = -KERNEL_SIZE / 2; karnelStep <= KERNEL_SIZE / 2; ++karnelStep)
    {
        uvOffset = texcoord;
        uvOffset.x += karnelStep * texOffset.x * dir.x;
        uvOffset.y += karnelStep * texOffset.y * dir.y;
        
        weight = Gaussian(karnelStep, 2.0f);
        
        result.xyz += BloomExtract(uvOffset).xyz * weight;
        
        sum += weight;
        
    }
    
    result *= (1.0f / sum);
    
    return result;
}

float4 SquareGaussianBlur(float2 texcoord, float2 texSize)
{
    // 9x9�̎l�p�`�J�[�l���̃I�t�Z�b�g
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

    float2 texOffset = float2(rcp(texSize.x), rcp(texSize.y)); // 1�s�N�Z���̒���
    float4 result = float4(0.0, 0.0, 0.0, 0.0); // ���ʂ̏�����
    float sum = 0.0f; // �d�݂̍��v

    for (int i = 0; i < 81; i++)
    {
        // �T���v���ʒu�̍��W
        float2 sampleCoord = texcoord + offsets[i] * texOffset * 1;

        // �K�E�V�A���d�݁i���S����̋����Ɋ�Â��j
        float weight = Gaussian(length(offsets[i]), gVignetteRedBloomParam.sigma);

        // �T���v���Əd�݂����Z
        result.xyz += BloomExtract(sampleCoord).xyz * weight;
        sum += weight;
    }

    // ���K��
    result *= (1.0f / sum);
    
    return result;
}

float3 VignetteRed(float4 color, float2 texCoord)
{
    float2 correct = texCoord * (1.0f - texCoord.xy);
    
    float vignette = correct.x * correct.y * gVignetteRedBloomParam.range;
    
    vignette = saturate(pow(vignette, gVignetteRedBloomParam.power));
    
    // �Â��Ȃ镔���ɐԐF��ǉ�
    float3 redTint = float3(1.0f, 0.0f, 0.0f); // �ԐF
    return lerp(color.rgb, redTint, 1.0f - vignette); // vignette�ŐF����
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texCoord);
    output.color.a = 1.0f;
    
    float2 texSize;
    gTexture.GetDimensions(texSize.x, texSize.y);
    
    float4 bloomColor = SquareGaussianBlur(input.texCoord, texSize);
    
    float3 VignetteColor = VignetteRed(bloomColor, input.texCoord);
    
    bloomColor.rgb *= gVignetteRedBloomParam.intensity;
    
    bloomColor.rgb += output.color.rgb;

    bloomColor.rgb += VignetteColor;
    
    return bloomColor;

}