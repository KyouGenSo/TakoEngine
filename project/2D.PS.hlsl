struct Color
{
    float4 color;
};

ConstantBuffer<Color> gColor : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main()
{
    PixelShaderOutput output;
    
    output.color = gColor.color;
    
    return output;
}