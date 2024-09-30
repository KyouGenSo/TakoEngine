struct VertexShaderOutput
{
    float32_t4 pos : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};