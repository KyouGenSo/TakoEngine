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
Texture2D<float4> gTexture : register(t0);            // �V�[���̃J���[�e�N�X�`��
Texture2D gDepthTexture : register(t1);               // �V�[���̐[�x�e�N�X�`��
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
    float minThreshold = gBloomParam.threshold - 0.1f;
    float maxThreshold = gBloomParam.threshold;
    // smoothstep�Ŋ��炩��臒l�K�p
    float brightness = max(color.r, max(color.g, color.b));
    float factor = smoothstep(minThreshold, maxThreshold, brightness);
    return color * factor;
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
        float weight = Gaussian(length(offsets[i]), gBloomParam.sigma);

        // �T���v���Əd�݂����Z
        result.xyz += BloomExtract(sampleCoord).xyz * weight;
        sum += weight;
    }

    // ���K��
    result *= (1.0f / sum);
    
    return result;
}

float4 FogColor(float2 texcoord)
{
    // �V�[���J���[���擾
    float4 sceneColor = gTexture.Sample(gSampler, texcoord);

    // �[�x�o�b�t�@����̐[�x�l���擾
    float depth = gDepthTexture.Sample(gSampler, texcoord).r;

    // �[�x�l�����j�A��
    float linearDepth = gCamera.nearPlane * gCamera.farPlane / (gCamera.farPlane - depth * (gCamera.farPlane - gCamera.nearPlane));

    // �̐σt�H�O�̃t�@�N�^�[���v�Z
    float fogFactor = exp(-linearDepth * gFogParam.density);
    fogFactor = saturate(fogFactor); // [0, 1]�͈̔͂ɃN�����v

    // �V�[���̃J���[�ƃt�H�O�̐F����
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