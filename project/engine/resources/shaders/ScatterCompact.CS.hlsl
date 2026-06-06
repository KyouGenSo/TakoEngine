// ============================================================================
// ScatterCompact.CS.hlsl
//   1M プール内に散在する生存パーティクルの index を、所属エミッターごとの
//   連続領域 [base_e, base_e + count_e) に詰め直して drawIndexList を構築する。
//   描画 VS は per-instance VBV 経由でこの index を引く。
//   生存条件は IntegrateAll.CS のカウント条件 (lifeRatio < 1 で alpha > 0) と一致。
// ============================================================================
#include "Particle.hlsli" // Particle, kMaxParticles, kMaxEmitters

// particleResource は IntegrateAll 直後も UAV state のまま。読み取り専用だが UAV としてバインドする。
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<uint> gDrawIndexList : register(u1); // 出力: 詰め直した生存 index 配列
RWStructuredBuffer<uint> gScatterCursor : register(u2); // per-emitter 書き込みカーソル (初期値 = base_e)

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint i = DTid.x;
    if (i >= kMaxParticles)
    {
        return;
    }

    // 生存していないパーティクルはスキップ
    if (!(gParticles[i].startColor.a > 0.0f && gParticles[i].endColor.a > 0.0f))
    {
        return;
    }

    uint e = gParticles[i].emitterId;
    if (e >= kMaxEmitters)
    {
        return; // 通常 emit 時に emitterId < kMaxEmitters が保証されるが安全策
    }

    // 所属エミッターの領域に連続スロットを確保して書き込む
    uint slot;
    InterlockedAdd(gScatterCursor[e], 1, slot);
    gDrawIndexList[slot] = i;
}
