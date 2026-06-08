// ============================================================================
// GPUParticle.VS.hlsl
//   全パーティクル共通の頂点シェーダ。各パーティクルを「描画モデル」(既定=板ポリ、
//   または per-emitter 指定モデル)として描画する。頂点/インデックスは描画モデルの SRV から
//   SV_VertexID でプルする (programmable vertex pulling)。per-instance 頂点ストリーム(slot1)
//   からコンパクション済みパーティクル index を受け取り、その transform で描画する。
// ============================================================================
#include "Particle.hlsli"

// メッシュ頂点 SRV の要素レイアウト (Skinning.CS の Vertex と一致させること)
struct MeshVertex
{
    float4 pos;
    float2 texcoord;
    float3 normal;
};

StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<Emitter>  gEmitters  : register(t2); // ビルボード ON/OFF 判定
StructuredBuffer<MeshVertex> gMeshVertices : register(t3);
StructuredBuffer<uint>       gMeshIndices  : register(t4);
ConstantBuffer<PerView>    gPerView : register(b0);

struct VertexShaderInput
{
    // per-instance 頂点ストリーム(slot1): コンパクション済みパーティクル index。
    // ExecuteIndirect の StartInstanceLocation=base_e により IA がオフセット参照する。
    uint particleIndex : TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input, uint vertexId : SV_VertexID)
{
    VertexShaderOutput output;

    Particle particle = gParticles[input.particleIndex];

    // インデックス SRV → 頂点 SRV の順にプル
    uint vIdx = gMeshIndices[vertexId];
    MeshVertex mv = gMeshVertices[vIdx];

    // ビルボード判定
    bool useBillboard = true;
    uint eid = particle.emitterId;
    if (eid < kMaxEmitters)
    {
        useBillboard = (gEmitters[eid].flags & EFLAG_BILLBOARD) != 0;
    }
    float4x4 identityMat = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    float4x4 worldMat = useBillboard ? gPerView.billboardMat : identityMat;

    // Z 回転 
    if (particle.rotate.z != 0.0f)
    {
        float s, c;
        sincos(particle.rotate.z, s, c);
        float3 right = worldMat[0].xyz;
        float3 up = worldMat[1].xyz;
        worldMat[0].xyz = right * c - up * s;
        worldMat[1].xyz = right * s + up * c;
    }

    // スケール (SCALE_FADE 対応)
    float lifeRatioForScale = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
    float3 currentScale = (particle.flags & PFLAG_SCALE_FADE)
        ? lerp(particle.scale, particle.endScale, lifeRatioForScale)
        : particle.scale;
    worldMat[0] *= currentScale.x;
    worldMat[1] *= currentScale.y;
    worldMat[2] *= currentScale.z;

    // 位置
    worldMat[3].xyz = particle.translate;

    output.pos = mul(mv.pos, mul(worldMat, gPerView.viewProj));
    output.texcoord = mv.texcoord;

    float lifeRatio = particle.currentTime / particle.lifeTime;
    output.color = lerp(particle.startColor, particle.endColor, lifeRatio);

    return output;
}