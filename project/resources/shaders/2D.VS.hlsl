struct VertexShaderInput
{
    float32_t2 pos : POSITION0;
};

struct VertexShaderOutput
{
    float32_t4 pos : SV_POSITION;
};

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    output.pos = mul(float32_t4(input.pos, 0.0f, 1.0f), gTransformationMatrix.WVP);

    return output;
}