// ============================================================================
// BuildDrawArgs.CS.hlsl
//   per-emitter の生存数を排他プレフィックスサムし、各エミッターの先頭オフセット
//   base_e を求め、ExecuteIndirect 用の DRAW 引数 (DrawInstanced) とスキャッタ用
//   カーソルを構築する。
//   エミッター数は kMaxEmitters (<=512) なので 512 スレッド 1 グループの単一スキャンで完結。
// ============================================================================
#include "Particle.hlsli" // kMaxEmitters

// D3D12_DRAW_ARGUMENTS と同一レイアウト (16 バイト)
struct IndirectDrawArgs
{
    uint vertexCountPerInstance;
    uint instanceCount;
    uint startVertexLocation;
    uint startInstanceLocation;
};

RWStructuredBuffer<uint> gPerEmitterCount : register(u0);       // 入力: per-emitter 生存数
RWStructuredBuffer<IndirectDrawArgs> gDrawArgs : register(u1);  // 出力: per-emitter Indirect 引数
RWStructuredBuffer<uint> gScatterCursor : register(u2);         // 出力: per-emitter スキャッタカーソル (= base_e)
StructuredBuffer<uint> gEmitterTemplate : register(t0);        // 入力: per-emitter の描画頂点数 (= 描画モデルの index 数。既定板ポリは 6)

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

        // DrawInstanced: VertexCountPerInstance は描画モデルの index 数
        // (gEmitterTemplate。既定板ポリは 6、カスタムモデルはその index 数)。
        // per-instance VBV の StartInstanceLocation=base_e で drawIndexList[base_e + SV_InstanceID]
        // を読み、VS が SV_VertexID から頂点/インデックスを SRV プルして描画する。
        IndirectDrawArgs args;
        args.vertexCountPerInstance = gEmitterTemplate[e];
        args.instanceCount = count;
        args.startVertexLocation = 0;
        args.startInstanceLocation = baseOffset;
        gDrawArgs[e] = args;
    }
}
