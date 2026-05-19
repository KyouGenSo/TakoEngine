#include "Particle.hlsli"
#include "ForceField.hlsli"
#include "PhysicsParams.hlsli"
#include "PerlinNoise3D.hlsli"
#include "DepthCollision.hlsli"

// リソースバインディング
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

StructuredBuffer<ForceField> gForceFields : register(t0);
Texture2D<float> gDepthBuffer : register(t1);
StructuredBuffer<Emitter> gEmitters : register(t2); // emitter 逆引き

SamplerState gDepthSampler : register(s0);

ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<PhysicsParams> gPhysicsParams : register(b1);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;

    // 有効な範囲のパーティクルのみ処理
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // アルファ値が 0 より大きいアクティブなパーティクルのみ更新
    if (!(gParticles[particleIndex].startColor.a > 0.0f && gParticles[particleIndex].endColor.a > 0.0f))
    {
        return;
    }

    // 現在の状態を読み取り
    float3 currentPos = gParticles[particleIndex].translate;
    float3 prevPos = gParticles[particleIndex].prevPosition;
    float dt = gPerFrame.deltaTime;

    // --- 加速度の計算 ---
    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

    // フォースフィールドの評価（パーティクルごとのフラグで制御）
    if (gParticles[particleIndex].flags & PFLAG_USE_FORCE_FIELD)
    {
        uint forceFieldCount = gPhysicsParams.activeForceFieldCount;
        for (uint i = 0; i < forceFieldCount; i++)
        {
            acceleration += EvaluateForceField(gForceFields[i], currentPos);
        }
    }

    // Per-Emitter Target 収束 (バネ-ダンパ)
    // 所属エミッターのフラグを引き、EFLAG_CONVERGE_TO_TARGET が立っていれば targetPosition へ向かう力を加算
    {
        uint eid = gParticles[particleIndex].emitterId;
        if (eid < gPerFrame.activeEmitterCount &&
            (gEmitters[eid].flags & EFLAG_CONVERGE_TO_TARGET))
        {
            float3 toTarget = gEmitters[eid].targetPosition - currentPos;
            float3 vel = (currentPos - prevPos) / max(dt, 0.0001f);
            acceleration += gEmitters[eid].convergeStiffness * toTarget
                          - gEmitters[eid].convergeDamping * vel;
        }
    }

    // Per-Particle Spawn 拘束 (各粒子が自分の targetLocal へバネ-ダンパで引き寄せられる)
    // Mesh エミッタの場合は targetLocal をメッシュ world で変換、それ以外は world 座標として直接使用
    {
        uint eid = gParticles[particleIndex].emitterId;
        if (eid < gPerFrame.activeEmitterCount &&
            (gEmitters[eid].flags & EFLAG_LOCK_TO_SPAWN))
        {
            float3 currentTarget;
            if (gEmitters[eid].type == EMITTER_TYPE_MESH)
            {
                // mesh local → world (engine 規約 mul(vec, matrix))
                currentTarget = mul(float4(gParticles[particleIndex].targetLocal, 1.0f), gEmitters[eid].meshWorld).xyz;
            }
            else
            {
                currentTarget = gParticles[particleIndex].targetLocal;
            }
            float3 toTarget = currentTarget - currentPos;
            float3 vel = (currentPos - prevPos) / max(dt, 0.0001f);
            acceleration += gEmitters[eid].lockStiffness * toTarget
                          - gEmitters[eid].lockDamping * vel;
        }
    }

    // --- Verlet 積分 ---
    // フレーム間変位から暗黙速度を導出
    float3 displacement = currentPos - prevPos;

    // 減衰の適用（エネルギー散逸） — per-emitter 値をパーティクルから読む
    displacement *= gParticles[particleIndex].damping;

    // Curl Noise乱流（速度の擾乱として displacement に加算。accelerationではなくdisplacementに適用することで十分な強度を得る）
    if (gParticles[particleIndex].flags & PFLAG_USE_CURL_NOISE)
    {
        // per-emitter 値をパーティクルから読む。noiseTime は全体の時間進行なのでグローバル維持
        float scale = gParticles[particleIndex].noiseScale;
        float3 noisePos = currentPos * scale + float3(0.0f, 0.0f, gPhysicsParams.noiseTime);
        displacement += curlNoise3D(noisePos) * gParticles[particleIndex].noiseStrength;
    }

    // 新しい位置を計算: newPos = pos + displacement + accel * dt²
    float3 newPos = currentPos + displacement + acceleration * dt * dt;

    // --- 深度バッファ衝突 ---
    if (gParticles[particleIndex].flags & PFLAG_USE_DEPTH_COLLISION)
    {
        float3 collisionPrev = currentPos;
        bool collided = ResolveDepthCollision(
            newPos, collisionPrev,
            gPhysicsParams.viewProj, gPhysicsParams.invViewProj,
            gPhysicsParams.screenSize, gPhysicsParams.cameraPos,
            gParticles[particleIndex].collisionRestitution, gParticles[particleIndex].particleRadius,
            gPhysicsParams.depthBias, gDepthBuffer, gDepthSampler);

        if (collided)
        {
            // 反射速度が collisionPrev にエンコード済み
            currentPos = collisionPrev;
        }
    }

    // 位置の更新
    gParticles[particleIndex].prevPosition = currentPos;
    gParticles[particleIndex].translate = newPos;

    // --- 寿命管理 ---
    // 経過時間の更新
    gParticles[particleIndex].currentTime += dt;

    // 寿命進行率 (0.0 → 1.0)。寿命終了判定はこの値で行うため、alpha フェード ON/OFF と独立
    float lifeRatio = gParticles[particleIndex].currentTime / max(gParticles[particleIndex].lifeTime, 0.0001f);

    // alpha フェード (PFLAG_ALPHA_FADE が立っているときのみ)
    // 立っていない場合は寿命中ずっと不透明 (alpha = 1)、寿命終端で一気に 0
    float alpha = (gParticles[particleIndex].flags & PFLAG_ALPHA_FADE)
        ? saturate(1.0f - lifeRatio)
        : 1.0f;
    gParticles[particleIndex].startColor.a = alpha;
    gParticles[particleIndex].endColor.a = alpha;

    // 寿命終了判定は lifeRatio ベース (alpha フェード OFF でも正しく動く)
    if (lifeRatio >= 1.0f)
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
