// ============================================================================
// BuildDrawArgs.CS.hlsl
//   per-emitter の生存数を排他プレフィックスサムし、各エミッターの先頭オフセット
//   base_e を求め、quad 用 (DrawIndexed) と mesh 用 (DrawInstanced) の
//   ExecuteIndirect 引数、およびスキャッタ用カーソルを構築する。
//   エミッター数は kMaxEmitters (<=512) なので 512 スレッド 1 グループの単一スキャンで完結。
// ============================================================================
#include "Particle.hlsli" // kMaxEmitters

// quad 用: D3D12_DRAW_INDEXED_ARGUMENTS と同一レイアウト (20 バイト)
struct IndirectDrawIndexedArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int  baseVertexLocation;
    uint startInstanceLocation;
};

// mesh 用: D3D12_DRAW_ARGUMENTS と同一レイアウト (16 バイト)
struct IndirectDrawArgs
{
    uint vertexCountPerInstance;
    uint instanceCount;
    uint startVertexLocation;
    uint startInstanceLocation;
};

RWStructuredBuffer<uint> gPerEmitterCount : register(u0);            // 入力: per-emitter 生存数
RWStructuredBuffer<IndirectDrawIndexedArgs> gDrawArgs : register(u1); // 出力: quad 用 Indirect 引数
RWStructuredBuffer<uint> gScatterCursor : register(u2);             // 出力: per-emitter スキャッタカーソル (= base_e)
RWStructuredBuffer<IndirectDrawArgs> gMeshDrawArgs : register(u3);    // 出力: mesh 用 Indirect 引数
StructuredBuffer<uint> gEmitterTemplate : register(t0);              // 入力: per-emitter メッシュ頂点数 (quad は 0)

// クアッド (2 三角形 = 6 インデックス) 固定
static const uint kQuadIndexCount = 6;

groupshared uint gsScan[512];

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    uint e = GI;

    uint count = (e < kMaxEmitters) ? gPerEmitterCount[e] : 0u;
    gsScan[e] = count;
    GroupMemoryBarrierWithGroupSync();

    // Hillis-Steele inclusive scan (512 要素 → 9 ステップ)
    [unroll]
    for (uint offset = 1u; offset < 512u; offset <<= 1)
    {
        uint v = (e >= offset) ? gsScan[e - offset] : 0u;
        GroupMemoryBarrierWithGroupSync();
        gsScan[e] += v;
        GroupMemoryBarrierWithGroupSync();
    }

    uint baseOffset = gsScan[e] - count; // exclusive prefix sum

    if (e < kMaxEmitters)
    {
        gScatterCursor[e] = baseOffset;

        // quad 用 (DrawIndexed): per-instance VBV の StartInstanceLocation=base_e で
        // drawIndexList[base_e + SV_InstanceID] を読む。
        IndirectDrawIndexedArgs qa;
        qa.indexCountPerInstance = kQuadIndexCount;
        qa.instanceCount = count;
        qa.startIndexLocation = 0;
        qa.baseVertexLocation = 0;
        qa.startInstanceLocation = baseOffset;
        gDrawArgs[e] = qa;

        // mesh 用 (DrawInstanced 非indexed): VertexCountPerInstance はメッシュ index 数
        // (テンプレート。quad エミッターは 0 で no-op)。
        IndirectDrawArgs ma;
        ma.vertexCountPerInstance = gEmitterTemplate[e];
        ma.instanceCount = count;
        ma.startVertexLocation = 0;
        ma.startInstanceLocation = baseOffset;
        gMeshDrawArgs[e] = ma;
    }
}
