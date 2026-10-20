// 描画物の maskId を R8_UINT の ObjectMask へ書く
struct MaskParam
{
    uint maskId;
};

ConstantBuffer<MaskParam> gMaskParam : register(b0);

uint main() : SV_TARGET
{
    return gMaskParam.maskId;
}
