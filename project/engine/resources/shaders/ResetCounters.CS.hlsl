// ============================================================================
// ResetCounters.CS.hlsl
//   毎フレーム、per-emitter の生存数カウンタ配列を 0 クリアするコンピュートシェーダ。
//   IntegrateAll.CS が InterlockedAdd で加算する前に実行する。
//   512 スレッド 1 グループ (Dispatch(1,1,1)) で kMaxEmitters 要素をクリアする。
// ============================================================================
#include "Particle.hlsli" // kMaxEmitters

// per-emitter 生存パーティクル数 (kMaxEmitters 要素)
RWStructuredBuffer<uint> gPerEmitterCount : register(u0);

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint e = DTid.x;
    if (e < kMaxEmitters)
    {
        gPerEmitterCount[e] = 0;
    }
}
