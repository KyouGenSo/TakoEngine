#include "Particle.hlsli"

// リソースバインディング
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    
    // 有効な範囲のパーティクルのみ処理
    if (particleIndex < kMaxParticles)
    {
        // アルファ値が0より大きい＝アクティブなパーティクルのみ更新
        if (any((gParticles[particleIndex].startColor.a > 0.0f) && (gParticles[particleIndex].endColor.a > 0.0f)))
        {
            // 位置の更新
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity;
            
            // 経過時間の更新
            gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
            
            // 寿命に基づいてアルファ値を計算
            float alpha = 1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            gParticles[particleIndex].startColor.a = saturate(alpha);
            gParticles[particleIndex].endColor.a = saturate(alpha);
            
            // 寿命切れならフリーリストに戻す処理
            if (alpha <= 0.0f)
            {
                gParticles[particleIndex].startColor.a = 0.0f;
                gParticles[particleIndex].endColor.a = 0.0f;
                gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
                
                // フリーリストに追加
                int freeListIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
                
                // 範囲チェック
                if (freeListIndex >= 0 && (freeListIndex + 1) < kMaxParticles)
                {
                    gFreeList[freeListIndex + 1] = particleIndex;
                }
                else
                {
                    // エラーケースの処理
                    InterlockedAdd(gFreeListIndex[0], -1);
                }
            }
        }
    }
}