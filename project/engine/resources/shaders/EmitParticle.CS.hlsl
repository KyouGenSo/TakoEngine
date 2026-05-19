#include "Particle.hlsli"
#include "Random.hlsli"

// パラメータごとのランダム化判定
// randomFlags が 0 の場合は旧来の「range != (0,0) ならランダム」自動判定にフォールバック
// 既存 JSON プリセットの後方互換を保つための分岐
bool ShouldRandomize(uint randomFlags, uint flagBit, float2 range)
{
    if (randomFlags != 0u)
    {
        return (randomFlags & flagBit) != 0u;
    }
    return any(range != float2(0.0f, 0.0f));
}

// 回転行列の計算
float3x3 CalculateRotationMatrix(float3 eulerAngles)
{
    // 度数からラジアンに変換
    float3 rad = eulerAngles * (3.14159265f / 180.0f);

    // X軸回転行列
    float3x3 rotX = float3x3(
        1.0f, 0.0f, 0.0f,
        0.0f, cos(rad.x), -sin(rad.x),
        0.0f, sin(rad.x), cos(rad.x)
    );

    // Y軸回転行列
    float3x3 rotY = float3x3(
        cos(rad.y), 0.0f, sin(rad.y),
        0.0f, 1.0f, 0.0f,
        -sin(rad.y), 0.0f, cos(rad.y)
    );

    // Z軸回転行列
    float3x3 rotZ = float3x3(
        cos(rad.z), -sin(rad.z), 0.0f,
        sin(rad.z), cos(rad.z), 0.0f,
        0.0f, 0.0f, 1.0f
    );

    // 行列の合成（Z*Y*X順）
    return mul(mul(rotZ, rotY), rotX);
}

// 球体エミッタからのランダム点生成（SpawnLocation 対応）
// - SPAWN_INSIDE  : 体積に比例した均一分布 (r = radius * rand^(1/3))
// - SPAWN_SURFACE : 球面上 (r = radius 固定)
// - SPAWN_EDGE    : 球には頂点が無いため Surface へフォールバック (UI で警告表示済み)
float3 GetRandomPointInSphere(RandomGenerator generator, float3 center, float radius, uint spawnLocation)
{
    // 方向ベクトルの生成
    float3 dir = generator.Generate3d() * 2.0f - 1.0f;
    float len = length(dir);

    // ゼロ除算防止
    if (len < 0.0001f)
    {
        dir = float3(0.0f, 1.0f, 0.0f);
        len = 1.0f;
    }

    // 正規化
    dir /= len;

    // SpawnLocation に応じた半径
    float r;
    if (spawnLocation == SPAWN_SURFACE || spawnLocation == SPAWN_EDGE)
    {
        r = radius;
    }
    else
    {
        // SPAWN_INSIDE: 球体内の均一分布のためのスケーリング（体積に比例）
        r = radius * pow(generator.Generate1d(), 1.0f / 3.0f);
    }

    return center + dir * r;
}

// 箱型エミッタからのランダム点生成（SpawnLocation 対応）
// - SPAWN_INSIDE  : 範囲内ランダム
// - SPAWN_SURFACE : 6 面のいずれかをランダム選択し、1 軸を ±size/2 に固定
// - SPAWN_EDGE    : 12 辺のいずれかをランダム選択 (1 軸が辺方向、残り 2 軸の符号で 4 通り × 3 軸 = 12 辺)
float3 GetRandomPointInBox(RandomGenerator generator, float3 center, float3 size, float3 rotation, uint spawnLocation)
{
    float3 localPoint;

    if (spawnLocation == SPAWN_SURFACE)
    {
        // 6 面: 軸 3 種 × 表裏 2 = 6
        localPoint = (generator.Generate3d() - 0.5f) * size;
        uint axis = (uint)(generator.Generate1d() * 3.0f);
        float sign = (generator.Generate1d() < 0.5f) ? -0.5f : 0.5f;
        if (axis == 0)      localPoint.x = sign * size.x;
        else if (axis == 1) localPoint.y = sign * size.y;
        else                localPoint.z = sign * size.z;
    }
    else if (spawnLocation == SPAWN_EDGE)
    {
        // 12 辺: 辺方向の軸 3 種 × 残り 2 軸の符号 2x2 = 12
        uint edgeAxis = (uint)(generator.Generate1d() * 3.0f);
        float r  = generator.Generate1d() - 0.5f; // 辺方向のパラメータ
        float s1 = (generator.Generate1d() < 0.5f) ? -0.5f : 0.5f;
        float s2 = (generator.Generate1d() < 0.5f) ? -0.5f : 0.5f;
        if (edgeAxis == 0)      localPoint = float3(r, s1, s2);
        else if (edgeAxis == 1) localPoint = float3(s1, r, s2);
        else                    localPoint = float3(s1, s2, r);
        localPoint *= size;
    }
    else
    {
        // SPAWN_INSIDE: ローカル座標系のランダムな点
        localPoint = (generator.Generate3d() - 0.5f) * size;
    }

    // 回転行列の適用
    float3x3 rotMatrix = CalculateRotationMatrix(rotation);
    float3 rotatedPoint = mul(rotMatrix, localPoint);

    return center + rotatedPoint;
}

// 三角形エミッタからのランダム点生成（SpawnLocation 対応）
// - SPAWN_SURFACE / SPAWN_INSIDE : バリ重心 (Inside は 2D 形状で意味なし → Surface にフォールバック)
// - SPAWN_EDGE                   : 3 辺いずれかランダム選択 + 辺上線形補間
float3 GetRandomPointOnTriangle(RandomGenerator generator, float3 p0, float3 p1, float3 p2, uint spawnLocation)
{
    if (spawnLocation == SPAWN_EDGE)
    {
        uint edgeIdx = (uint)(generator.Generate1d() * 3.0f);
        float t = generator.Generate1d();
        if (edgeIdx == 0) return lerp(p0, p1, t);
        if (edgeIdx == 1) return lerp(p1, p2, t);
        return lerp(p2, p0, t);
    }

    // バリセントリック座標を用いた三角形上のランダムな点
    float r1 = generator.Generate1d();
    float r2 = generator.Generate1d();

    // 三角形上の均一分布を確保
    if (r1 + r2 > 1.0f)
    {
        r1 = 1.0f - r1;
        r2 = 1.0f - r2;
    }

    float a = 1.0f - r1 - r2;
    float b = r1;
    float c = r2;

    return a * p0 + b * p1 + c * p2;
}

RWStructuredBuffer<Particle> gParticles : register(u0); // パーティクルバッファ
RWStructuredBuffer<int> gFreeListIndex : register(u1);  // フリーリストインデックス
RWStructuredBuffer<uint> gFreeList : register(u2);      // フリーリスト
StructuredBuffer<Emitter> gEmitters : register(t0);     // エミッターリスト

// Mesh エミッタ用 (固定スロット、Mesh エミッタが無い場合は dummy リソースで bind)
struct MeshVertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};
StructuredBuffer<MeshVertex> gMeshVertices    : register(t10);
StructuredBuffer<uint>       gMeshIndices     : register(t11);
// 要素数 = triCount + 1, prefixSum[0]=0, prefixSum[triCount]=totalArea
StructuredBuffer<float>      gMeshAreaPrefixSum : register(t12);

ConstantBuffer<PerFrame> gPerFrame : register(b0);

// 0xFFFFFFFF = 非 Mesh エミッタ向け Dispatch、それ以外 = 指定 index の Mesh エミッタのみ処理
cbuffer RootConstants : register(b1)
{
    uint gTargetMeshEmitterId;
};

// areaPrefixSumSrvIndex == 0 / totalArea == 0 のとき等確率分配にフォールバック (後方互換維持)
uint SelectTriangleByArea(uint triCount, float totalArea, uint areaPrefixSumSrvIndex, float rand01)
{
    if (areaPrefixSumSrvIndex != 0u && totalArea > 0.0f)
    {
        float target = rand01 * totalArea;
        uint lo = 0u;
        uint hi = triCount;
        [loop]
        for (uint iter = 0u; iter < 32u; ++iter)
        {
            if (lo >= hi) break;
            uint mid = (lo + hi) / 2u;
            if (gMeshAreaPrefixSum[mid + 1u] < target) lo = mid + 1u;
            else hi = mid;
        }
        return min(lo, triCount - 1u);
    }
    else
    {
        return min((uint)(rand01 * (float)triCount), triCount - 1u);
    }
}

[numthreads(16, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // エミッターのインデックス計算
    uint emitterIndex = DTid.x;

    // 範囲チェック
    if (emitterIndex >= gPerFrame.activeEmitterCount)
    {
        return;
    }

    // このエミッターがアクティブかチェック
    if (!(gEmitters[emitterIndex].flags & EFLAG_ACTIVE))
    {
        return;
    }

    // 射出フラグが立っているかチェック
    if (!(gEmitters[emitterIndex].flags & EFLAG_EMITTING))
    {
        return;
    }

    {
        const uint kInvalidMeshTarget = 0xFFFFFFFFu;
        uint emitterType = gEmitters[emitterIndex].type;
        if (gTargetMeshEmitterId == kInvalidMeshTarget) {
            if (emitterType == EMITTER_TYPE_MESH) return;
        } else {
            if (emitterIndex != gTargetMeshEmitterId) return;
        }
    }

    RandomGenerator generator;
    generator.seed = float3(
        DTid.x * 73.0f + gPerFrame.time * 173.5f,
        DTid.y * 191.0f + gPerFrame.time * 71.3f + gEmitters[emitterIndex].emitterID * 53.0f,
        gEmitters[emitterIndex].emitterID * 127.0f + gPerFrame.time * 257.1f
    );

    // このエミッターから指定数のパーティクルを射出
    for (uint particleIndex = 0; particleIndex < gEmitters[emitterIndex].count; ++particleIndex)
    {
        // FreeListから空きパーティクルスロットを取得
        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        // 有効なインデックスか十分な空きがあるかを確認
        if (freeListIndex >= 0 && freeListIndex < kMaxParticles && gFreeList[freeListIndex] < kMaxParticles)
        {
            // FreeListから実際のパーティクルIDを取得
            uint particleID = gFreeList[freeListIndex];

            // パーティクルの位置を決定（エミッター形状に応じて）
            float3 particlePosition;

            switch (gEmitters[emitterIndex].type)
            {
                case EMITTER_TYPE_SPHERE:
                    particlePosition = GetRandomPointInSphere(
                        generator,
                        gEmitters[emitterIndex].position,
                        gEmitters[emitterIndex].radius,
                        gEmitters[emitterIndex].spawnLocation
                    );
                    break;

                case EMITTER_TYPE_BOX:
                    particlePosition = GetRandomPointInBox(
                        generator,
                        gEmitters[emitterIndex].position,
                        gEmitters[emitterIndex].boxSize,
                        gEmitters[emitterIndex].boxRotation,
                        gEmitters[emitterIndex].spawnLocation
                    );
                    break;

                case EMITTER_TYPE_TRIANGLE:
                    particlePosition = GetRandomPointOnTriangle(
                        generator,
                        gEmitters[emitterIndex].position + gEmitters[emitterIndex].triangleV1,
                        gEmitters[emitterIndex].position + gEmitters[emitterIndex].triangleV2,
                        gEmitters[emitterIndex].position + gEmitters[emitterIndex].triangleV3,
                        gEmitters[emitterIndex].spawnLocation
                    );
                    break;

                case EMITTER_TYPE_MESH:
                {
                    // メッシュからのスポーン (Surface / Edge / Inside)
                    // Inside は SDF 未実装、AABB 内ランダムでフォールバック
                    float3 localSpawn = float3(0.0f, 0.0f, 0.0f);
                    uint triCount = gEmitters[emitterIndex].meshTriangleCount;
                    if (triCount > 0)
                    {
                        if (gEmitters[emitterIndex].spawnLocation == SPAWN_EDGE)
                        {
                            uint triIdx = SelectTriangleByArea(
                                triCount,
                                gEmitters[emitterIndex].meshTotalArea,
                                gEmitters[emitterIndex].meshAreaPrefixSumSrvIndex,
                                generator.Generate1d());
                            uint i0 = gMeshIndices[triIdx * 3 + 0];
                            uint i1 = gMeshIndices[triIdx * 3 + 1];
                            uint i2 = gMeshIndices[triIdx * 3 + 2];
                            float3 v0 = gMeshVertices[i0].position.xyz;
                            float3 v1 = gMeshVertices[i1].position.xyz;
                            float3 v2 = gMeshVertices[i2].position.xyz;
                            uint edgeIdx = (uint)(generator.Generate1d() * 3.0f);
                            float t = generator.Generate1d();
                            if (edgeIdx == 0)      localSpawn = lerp(v0, v1, t);
                            else if (edgeIdx == 1) localSpawn = lerp(v1, v2, t);
                            else                   localSpawn = lerp(v2, v0, t);
                        }
                        else if (gEmitters[emitterIndex].spawnLocation == SPAWN_INSIDE)
                        {
                            // SDF 未実装、AABB 内ランダムでフォールバック
                            float3 t3 = float3(generator.Generate1d(), generator.Generate1d(), generator.Generate1d());
                            localSpawn = lerp(gEmitters[emitterIndex].meshAabbMin, gEmitters[emitterIndex].meshAabbMax, t3);
                        }
                        else
                        {
                            // SPAWN_SURFACE
                            uint triIdx = SelectTriangleByArea(
                                triCount,
                                gEmitters[emitterIndex].meshTotalArea,
                                gEmitters[emitterIndex].meshAreaPrefixSumSrvIndex,
                                generator.Generate1d());
                            uint i0 = gMeshIndices[triIdx * 3 + 0];
                            uint i1 = gMeshIndices[triIdx * 3 + 1];
                            uint i2 = gMeshIndices[triIdx * 3 + 2];
                            float3 v0 = gMeshVertices[i0].position.xyz;
                            float3 v1 = gMeshVertices[i1].position.xyz;
                            float3 v2 = gMeshVertices[i2].position.xyz;
                            float u = generator.Generate1d();
                            float v = generator.Generate1d();
                            if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
                            localSpawn = v0 + u * (v1 - v0) + v * (v2 - v0);
                        }
                    }
                    // mesh local → world 変換 (engine 規約は mul(vec, matrix))
                    particlePosition = mul(float4(localSpawn, 1.0f), gEmitters[emitterIndex].meshWorld).xyz;
                    // local 座標を保存 (per-particle 拘束で参照)
                    gParticles[particleID].targetLocal = localSpawn;
                    break;
                }

                default:
                    // デフォルトはエミッターの中心
                    particlePosition = gEmitters[emitterIndex].position;
                    break;
            }

            // -----------------------------パーティクルの初期化----------------------------- //

            // サイズ設定---------------------------------------------------------------------------------
            float3 particleScale;

            // X方向のスケール
            if (ShouldRandomize(gEmitters[emitterIndex].randomFlags, ERAND_SCALE_X, gEmitters[emitterIndex].scaleRangeX))
            {
                particleScale.x = generator.Generate1d() * (gEmitters[emitterIndex].scaleRangeX.y - gEmitters[emitterIndex].scaleRangeX.x) + gEmitters[emitterIndex].scaleRangeX.x;
            }
            else
            {
                particleScale.x = 1.0f;
            }

            // Y方向のスケール
            if (ShouldRandomize(gEmitters[emitterIndex].randomFlags, ERAND_SCALE_Y, gEmitters[emitterIndex].scaleRangeY))
            {
                particleScale.y = generator.Generate1d() * (gEmitters[emitterIndex].scaleRangeY.y - gEmitters[emitterIndex].scaleRangeY.x) + gEmitters[emitterIndex].scaleRangeY.x;
            }
            else
            {
                particleScale.y = 1.0f;
            }

            // Z方向のスケール
            particleScale.z = 1.0f;

            // パーティクルのスケールを設定
            gParticles[particleID].scale.x = particleScale.x;
            gParticles[particleID].scale.y = particleScale.y;
            gParticles[particleID].scale.z = particleScale.z;

            // 終了時スケール (スケール縮小消滅)
            // EFLAG_USE_SCALE_FADE が立っていれば endScaleDefault に向けて補間、
            // 立っていなければ scale 自身を入れて補間しても変化なし
            if (gEmitters[emitterIndex].flags & EFLAG_USE_SCALE_FADE)
            {
                gParticles[particleID].endScale = gEmitters[emitterIndex].endScaleDefault;
            }
            else
            {
                gParticles[particleID].endScale = particleScale;
            }

            // 位置設定---------------------------------------------------------------------------------
            gParticles[particleID].translate = particlePosition;

            // Per-Particle Spawn 拘束用に targetLocal を保存
            // Mesh の場合は Mesh case 内で mesh local 座標が既に書き込まれているので上書きしない
            if (gEmitters[emitterIndex].type != EMITTER_TYPE_MESH)
            {
                gParticles[particleID].targetLocal = particlePosition;
            }

            // 質量・セルインデックス・フォースフィールドフラグ設定----------------------------------
            gParticles[particleID].mass = 1.0f;
            gParticles[particleID].cellIndex = 0xFFFFFFFF; // 未割り当て
            gParticles[particleID].flags = 0;
            if (gEmitters[emitterIndex].flags & EFLAG_USE_FORCE_FIELD)
                gParticles[particleID].flags |= PFLAG_USE_FORCE_FIELD;
            if (gEmitters[emitterIndex].flags & EFLAG_USE_CURL_NOISE)
                gParticles[particleID].flags |= PFLAG_USE_CURL_NOISE;
            if (gEmitters[emitterIndex].flags & EFLAG_USE_DEPTH_COLLISION)
                gParticles[particleID].flags |= PFLAG_USE_DEPTH_COLLISION;
            if (gEmitters[emitterIndex].flags & EFLAG_USE_SCALE_FADE)
                gParticles[particleID].flags |= PFLAG_SCALE_FADE;
            if (gEmitters[emitterIndex].flags & EFLAG_USE_ALPHA_FADE)
                gParticles[particleID].flags |= PFLAG_ALPHA_FADE;

            // per-emitter 物理 / Curl Noise パラメーターをパーティクルへキャッシュ
            gParticles[particleID].damping              = gEmitters[emitterIndex].damping;
            gParticles[particleID].collisionRestitution = gEmitters[emitterIndex].collisionRestitution;
            gParticles[particleID].particleRadius       = gEmitters[emitterIndex].particleRadius;
            gParticles[particleID].noiseScale           = gEmitters[emitterIndex].noiseScale;
            gParticles[particleID].noiseStrength        = gEmitters[emitterIndex].noiseStrength;
            // エミッター配列インデックスを保持し、IntegrateAll.CS で逆引き可能に
            gParticles[particleID].emitterId            = emitterIndex;

            // 回転設定---------------------------------------------------------------------------------
            if (gEmitters[emitterIndex].flags & EFLAG_RANDOM_ROTATE_Z)
            {
                // ランダム回転フラグが立っている場合、ランダムな回転を設定
                gParticles[particleID].rotate.z = generator.Generate1d() * 360.0f; // Z軸回転
            }
            else
            {
                gParticles[particleID].rotate.z = 0.0f; // Z軸回転なし
            }


            // 速度設定---------------------------------------------------------------------------------
            float3 particleVelocity;

            float3 randomVel = (generator.Generate3d() * 2.0f - 1.0f) * 0.1f;

            // X方向の速度
            if (ShouldRandomize(gEmitters[emitterIndex].randomFlags, ERAND_VEL_X, gEmitters[emitterIndex].velRangeX))
            {
                particleVelocity.x = generator.Generate1d() * (gEmitters[emitterIndex].velRangeX.y - gEmitters[emitterIndex].velRangeX.x) + gEmitters[emitterIndex].velRangeX.x;
            }
            else
            {
                particleVelocity.x = randomVel.x;
            }

            // Y方向の速度
            if (ShouldRandomize(gEmitters[emitterIndex].randomFlags, ERAND_VEL_Y, gEmitters[emitterIndex].velRangeY))
            {
                particleVelocity.y = generator.Generate1d() * (gEmitters[emitterIndex].velRangeY.y - gEmitters[emitterIndex].velRangeY.x) + gEmitters[emitterIndex].velRangeY.x;
            }
            else
            {
                particleVelocity.y = randomVel.y;
            }

            // Z方向の速度
            if (ShouldRandomize(gEmitters[emitterIndex].randomFlags, ERAND_VEL_Z, gEmitters[emitterIndex].velRangeZ))
            {
                particleVelocity.z = generator.Generate1d() * (gEmitters[emitterIndex].velRangeZ.y - gEmitters[emitterIndex].velRangeZ.x) + gEmitters[emitterIndex].velRangeZ.x;
            }
            else
            {
                particleVelocity.z = randomVel.z;
            }

            // 正規化
            if (gEmitters[emitterIndex].flags & EFLAG_NORMALIZE)
            {
                // 正規化フラグが立っている場合、速度を正規化
                particleVelocity = normalize(particleVelocity);
            }

            // パーティクルの速度を設定
            gParticles[particleID].velocity.x = particleVelocity.x;
            gParticles[particleID].velocity.y = particleVelocity.y;
            gParticles[particleID].velocity.z = particleVelocity.z;

            // 前フレーム位置の初期化（Verlet積分用）
            // prevPosition = translate - velocity で初速をエンコード
            gParticles[particleID].prevPosition = particlePosition - particleVelocity;


            // 寿命設定---------------------------------------------------------------------------------
            if (ShouldRandomize(gEmitters[emitterIndex].randomFlags, ERAND_LIFETIME, gEmitters[emitterIndex].lifeTimeRange))
            {
                gParticles[particleID].lifeTime = generator.Generate1d() * (gEmitters[emitterIndex].lifeTimeRange.y - gEmitters[emitterIndex].lifeTimeRange.x) + gEmitters[emitterIndex].lifeTimeRange.x;
            }
            else
            {
                gParticles[particleID].lifeTime = 1.0f + generator.Generate1d() * 0.5f; // 1.0〜1.5秒
            }

            gParticles[particleID].currentTime = 0.0f;

            // 色設定---------------------------------------------------------------------------------
            gParticles[particleID].startColor.rgb = gEmitters[emitterIndex].startColorTint.rgb;
            gParticles[particleID].startColor.a = 1.0f;

            gParticles[particleID].endColor.rgb = gEmitters[emitterIndex].endColorTint.rgb;
            gParticles[particleID].endColor.a = 1.0f;

        }
        else
        {
            // 空きパーティクルがない場合は戻して終了
            InterlockedAdd(gFreeListIndex[0], 1);
            break;
        }
    }
}
