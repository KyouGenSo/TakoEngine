#include "Particle.hlsli"
#include "Random.hlsli"

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
    
    // 行列の合成（Z→Y→X順）
    return mul(mul(rotZ, rotY), rotX);
}

// 球体内のランダムな点を生成
float3 GetRandomPointInSphere(RandomGenerator generator, float3 center, float radius)
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
    
    // 球体内の均一分布のためのスケーリング（体積に比例）
    float r = radius * pow(generator.Generate1d(), 1.0f / 3.0f);
    
    return center + dir * r;
}

// 箱内のランダムな点を生成
float3 GetRandomPointInBox(RandomGenerator generator, float3 center, float3 size, float3 rotation)
{
    // ローカル座標内のランダムな点
    float3 localPoint = (generator.Generate3d() - 0.5f) * size;
    
    // 回転行列の適用
    float3x3 rotMatrix = CalculateRotationMatrix(rotation);
    float3 rotatedPoint = mul(rotMatrix, localPoint);
    
    return center + rotatedPoint;
}

// 三角形上のランダムな点を生成
float3 GetRandomPointOnTriangle(RandomGenerator generator, float3 p0, float3 p1, float3 p2)
{
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

ConstantBuffer<PerFrame> gPerFrame : register(b0);      // フレーム情報

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
    if (gEmitters[emitterIndex].isActive == 0)
    {
        return;
    }
    
    // 射出フラグが立っているかチェック
    if (gEmitters[emitterIndex].isEmit == 0)
    {
        return;
    }
    
    // 乱数生成器の初期化
    RandomGenerator generator;
    // シード値生成 (時間＋エミッターID＋スレッドID）
    generator.seed = float3(
        DTid.x + gPerFrame.time * 0.1f,
        DTid.y + gPerFrame.time * 0.2f,
        gEmitters[emitterIndex].emitterID + gPerFrame.time * 0.3f
    );
    
    // このエミッターから指定数のパーティクルを射出
    for (uint particleIndex = 0; particleIndex < gEmitters[emitterIndex].count; ++particleIndex)
    {
        // FreeListから空きパーティクルスロットを取得
        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        
        // 有効なインデックスかつ十分な空きがあるかを確認
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
                        gEmitters[emitterIndex].radius
                    );
                    break;
                    
                case EMITTER_TYPE_BOX:
                    particlePosition = GetRandomPointInBox(
                        generator,
                        gEmitters[emitterIndex].position,
                        gEmitters[emitterIndex].boxSize,
                        gEmitters[emitterIndex].boxRotation
                    );
                    break;
                    
                case EMITTER_TYPE_TRIANGLE:
                    particlePosition = GetRandomPointOnTriangle(
                        generator,
                        gEmitters[emitterIndex].position + gEmitters[emitterIndex].triangleV1,
                        gEmitters[emitterIndex].position + gEmitters[emitterIndex].triangleV2,
                        gEmitters[emitterIndex].position + gEmitters[emitterIndex].triangleV3
                    );
                    break;
                    
                default:
                    // デフォルトはエミッターの中心
                    particlePosition = gEmitters[emitterIndex].position;
                    break;
            }
            
            // -----------------------------パーティクルの初期化----------------------------- //
            
            // サイズ設定---------------------------------------------------------------------------------
            float3 particleScale;
            
            // X軸方向のスケール
            if (any(gEmitters[emitterIndex].scaleRangeX != float2(0.0f, 0.0f)))
            {
                particleScale.x = generator.Generate1d() * (gEmitters[emitterIndex].scaleRangeX.y - gEmitters[emitterIndex].scaleRangeX.x) + gEmitters[emitterIndex].scaleRangeX.x;
            }
            else
            {
                particleScale.x = 1.0f;
            }
            
            // Y軸方向のスケール
            if (any(gEmitters[emitterIndex].scaleRangeY != float2(0.0f, 0.0f)))
            {
                particleScale.y = generator.Generate1d() * (gEmitters[emitterIndex].scaleRangeY.y - gEmitters[emitterIndex].scaleRangeY.x) + gEmitters[emitterIndex].scaleRangeY.x;
            }
            else
            {
                particleScale.y = 1.0f;
            }
            
            // Z軸方向のスケール
            particleScale.z = 1.0f;
            
            // パーティクルのスケールを設定
            gParticles[particleID].scale.x = particleScale.x;
            gParticles[particleID].scale.y = particleScale.y;
            
            // 位置設定---------------------------------------------------------------------------------
            gParticles[particleID].translate = particlePosition;

        	// 回転設定---------------------------------------------------------------------------------
            if (gEmitters[emitterIndex].isRandomRotateZ > 0)
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

            // X軸方向の速度
            if (any(gEmitters[emitterIndex].velRangeX != float2(0.0f, 0.0f)))
            {
                particleVelocity.x = generator.Generate1d() * (gEmitters[emitterIndex].velRangeX.y - gEmitters[emitterIndex].velRangeX.x) + gEmitters[emitterIndex].velRangeX.x;
            }
            else
            {
                particleVelocity.x = randomVel.x;
            }

            // Y軸方向の速度
            if (any(gEmitters[emitterIndex].velRangeY != float2(0.0f, 0.0f)))
            {
                particleVelocity.y = generator.Generate1d() * (gEmitters[emitterIndex].velRangeY.y - gEmitters[emitterIndex].velRangeY.x) + gEmitters[emitterIndex].velRangeY.x;
            }
			else
			{
                particleVelocity.y = randomVel.y;
            }

            // Z軸方向の速度
            if (any(gEmitters[emitterIndex].velRangeZ != float2(0.0f, 0.0f)))
            {
                particleVelocity.z = generator.Generate1d() * (gEmitters[emitterIndex].velRangeZ.y - gEmitters[emitterIndex].velRangeZ.x) + gEmitters[emitterIndex].velRangeZ.x;
            }
            else
            {
                particleVelocity.z = randomVel.z;

            }

            // 正規化
            if (gEmitters[emitterIndex].isNormalize == 1)
            {
                // 正規化フラグが立っている場合、速度を正規化
                particleVelocity = normalize(particleVelocity);
            }

            // パーティクルの速度を設定
            gParticles[particleID].velocity.x = particleVelocity.x;
            gParticles[particleID].velocity.y = particleVelocity.y;
            gParticles[particleID].velocity.z = particleVelocity.z;
            
            
            // 寿命設定---------------------------------------------------------------------------------
            if (any(gEmitters[emitterIndex].lifeTimeRange != float2(0.0f, 0.0f)))
            {
                gParticles[particleID].lifeTime = generator.Generate1d() * (gEmitters[emitterIndex].lifeTimeRange.y - gEmitters[emitterIndex].lifeTimeRange.x) + gEmitters[emitterIndex].lifeTimeRange.x;
            }
            else
            {
                gParticles[particleID].lifeTime = 1.0f + generator.Generate1d() * 0.5f; // 1.0～1.5秒
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
            // 空きパーティクルが足りない場合は戻して終了
            InterlockedAdd(gFreeListIndex[0], 1);
            break;
        }
    }
}