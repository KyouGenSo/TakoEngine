#ifndef TAKO_COMMON_HLSLI
#define TAKO_COMMON_HLSLI

// 全ルートシグネチャに入る固定 static sampler（ShaderBindingLayout.cpp の MakeStaticSamplers と一致させる）
SamplerState           gLinearWrap  : register(s0);
SamplerState           gLinearClamp : register(s1);
SamplerState           gPointWrap   : register(s2);
SamplerState           gPointClamp  : register(s3);
SamplerComparisonState gShadowCmp   : register(s4);

#endif
