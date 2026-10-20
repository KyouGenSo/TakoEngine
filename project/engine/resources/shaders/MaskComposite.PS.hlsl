// ObjectMask が maskId と一致する画素はエフェクト後、それ以外はエフェクト前の色にする（3 枚とも同じ解像度）
#include "FullScreen.hlsli"

struct MaskParam
{
    uint maskId;
};

ConstantBuffer<MaskParam> gMaskParam : register(b0);
Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gEffected : register(t1);
Texture2D<uint> gObjectMask : register(t2);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    const int3 pixel = int3(input.position.xy, 0);
    return gObjectMask.Load(pixel) == gMaskParam.maskId ? gEffected.Load(pixel) : gTexture.Load(pixel);
}
