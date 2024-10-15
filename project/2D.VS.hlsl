struct VertexShaderInput
{
    float32_t2 pos : POSITION0;
};

struct VertexShaderOutput
{
    float32_t2 pos : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    output.pos = input.pos;

    return output;
}