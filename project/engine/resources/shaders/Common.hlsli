#ifndef TAKO_COMMON_HLSLI
#define TAKO_COMMON_HLSLI

// 全ルートシグネチャに入る固定 static sampler（ShaderBindingLayout.cpp の MakeStaticSamplers と一致させる）
SamplerState           gLinearWrap  : register(s0);
SamplerState           gLinearClamp : register(s1);
SamplerState           gPointWrap   : register(s2);
SamplerState           gPointClamp  : register(s3);
SamplerComparisonState gShadowCmp   : register(s4);

// sRGB の規格の区分式（C++ の Color::ToLinear / ToSRGB の 1 以下の部分と同じ）
float SRGBToLinear(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float LinearToSRGB(float c) { return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
float3 SRGBToLinear(float3 c) { return float3(SRGBToLinear(c.r), SRGBToLinear(c.g), SRGBToLinear(c.b)); }
float3 LinearToSRGB(float3 c) { return float3(LinearToSRGB(c.r), LinearToSRGB(c.g), LinearToSRGB(c.b)); }

#endif
