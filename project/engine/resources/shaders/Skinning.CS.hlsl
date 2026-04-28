struct Vertex
{
    float4 pos;
    float2 texcoord;
    float3 normal;
};

struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceMatrixIT;
};

struct VertexInfluence
{
    float4 weight;
    int4 index;
};

struct SkinningInfo
{
    uint numVertices;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);

RWStructuredBuffer<Vertex> gOutputVertices : register(u0);

ConstantBuffer<SkinningInfo> gSkinningInfo : register(b0);

[numthreads(1024, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint vertexIndex = DTid.x;
    if (vertexIndex < gSkinningInfo.numVertices)
    {
        Vertex inputVertex = gInputVertices[vertexIndex];
        VertexInfluence influence = gInfluences[vertexIndex];
        
        Vertex skinned;
        skinned.texcoord = inputVertex.texcoord;
        
        skinned.pos = mul(inputVertex.pos, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x;
        skinned.pos += mul(inputVertex.pos, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y;
        skinned.pos += mul(inputVertex.pos, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z;
        skinned.pos += mul(inputVertex.pos, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;
        skinned.pos.w = 1.0f;
        
        skinned.normal = mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.x].skeletonSpaceMatrixIT) * influence.weight.x;
        skinned.normal += mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.y].skeletonSpaceMatrixIT) * influence.weight.y;
        skinned.normal += mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.z].skeletonSpaceMatrixIT) * influence.weight.z;
        skinned.normal += mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.w].skeletonSpaceMatrixIT) * influence.weight.w;
        skinned.normal = normalize(skinned.normal);
        
        gOutputVertices[vertexIndex] = skinned;
    }
}