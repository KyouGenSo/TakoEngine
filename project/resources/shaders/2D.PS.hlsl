
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct VertexShaderOutput
{
    float32_t4 pos : SV_POSITION;
    float32_t4 color : COLOR0;
};

PixelShaderOutput main(VertexShaderOutput input) // VertexShaderOutput is the same as VertexShaderOutput in 2D.VS.hlsl
{
    PixelShaderOutput output;
    
    output.color = input.color;
    
    return output;
}