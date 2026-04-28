#include "FullScreen.hlsli"

static const uint kVertexNum = 3;
static const float4 kPositions[kVertexNum] = {
    { -1.0f, 1.0f, 0.0f, 1.0f },
    {  3.0f, 1.0f, 0.0f, 1.0f },
    { -1.0f, -3.0f, 0.0f, 1.0f } 
};

static const float2 kTexCoords[kVertexNum] = {
    { 0.0f, 0.0f },
    { 2.0f, 0.0f },
    { 0.0f, 2.0f }
};

VertexShaderOutput main(uint vertexID : SV_VertexID)
{
    VertexShaderOutput output;
    output.position = kPositions[vertexID];
    output.texCoord = kTexCoords[vertexID];
    return output;
}