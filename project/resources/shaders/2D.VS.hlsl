struct VertexShaderInput
{
    float2 pos : POSITION0;
    float4 color : COLOR0;
};

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
};

struct TransformationMatrix
{
    float32_t4x4 WVP;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    output.pos = mul(float4(input.pos, 0.0f, 1.0f), gTransformationMatrix.WVP);
    output.color = input.color;

    return output;
}