#include "FullScreen.hlsli"

static const uint32_t kVertexNum = 3;
static const float32_t4 kPositions[kVertexNum] = {
    { -1.0f, 1.0f, 0.0f, 1.0f },
    {  3.0f, 1.0f, 0.0f, 1.0f },
    { -1.0f, -3.0f, 0.0f, 1.0f } 
};

static const float32_t2 kTexCoords[kVertexNum] = {
    { 0.0f, 0.0f },
    { 2.0f, 0.0f },
    { 0.0f, 2.0f }
};

VertexShaderOutput main(uint32_t vertexID : SV_VertexID)
{
    VertexShaderOutput output;
    output.position = kPositions[vertexID];
    output.texCoord = kTexCoords[vertexID];
    return output;
}