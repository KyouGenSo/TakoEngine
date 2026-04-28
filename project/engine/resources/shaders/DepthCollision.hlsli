#ifndef DEPTH_COLLISION_HLSLI
#define DEPTH_COLLISION_HLSLI

// ============================================================================
// 深度バッファ衝突モジュール
// パーティクルがシーンジオメトリを貫通しないよう、深度バッファから
// 推定した表面法線で反射バウンスさせる。
// ============================================================================

/// ワールド座標をスクリーンUV + NDC深度に変換
/// @return float3(u, v, ndcDepth)  u,v は [0,1] 範囲、ndcDepth は [0,1]（near=0, far=1）
float3 ProjectToScreen(float3 worldPos, float4x4 viewProj)
{
    float4 clip = mul(float4(worldPos, 1.0f), viewProj);
    float3 ndc = clip.xyz / clip.w;
    // NDC → UV: x: [-1,1] → [0,1],  y: [-1,1] → [1,0]（Y反転）
    return float3(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f, ndc.z);
}

/// スクリーンUV + 深度値からワールド座標を復元
float3 ReconstructWorldPos(float2 screenUV, float depth, float4x4 invViewProj)
{
    // UV → NDC: x: [0,1] → [-1,1],  y: [0,1] → [1,-1]（Y反転）
    float4 ndc = float4(
        screenUV.x * 2.0f - 1.0f,
        -(screenUV.y * 2.0f - 1.0f),
        depth,
        1.0f);
    float4 world = mul(ndc, invViewProj);
    return world.xyz / world.w;
}

/// 深度バッファから4点クロスパターンで表面法線を推定
float3 EstimateDepthNormal(
    float2 screenUV,
    float2 texelSize,
    float4x4 invViewProj,
    Texture2D<float> depthTex,
    SamplerState depthSamp)
{
    // 隣接4点の深度をサンプル
    float dL = depthTex.SampleLevel(depthSamp, screenUV + float2(-texelSize.x, 0.0f), 0);
    float dR = depthTex.SampleLevel(depthSamp, screenUV + float2( texelSize.x, 0.0f), 0);
    float dU = depthTex.SampleLevel(depthSamp, screenUV + float2(0.0f, -texelSize.y), 0);
    float dD = depthTex.SampleLevel(depthSamp, screenUV + float2(0.0f,  texelSize.y), 0);

    // 各点のワールド位置を復元
    float3 posL = ReconstructWorldPos(screenUV + float2(-texelSize.x, 0.0f), dL, invViewProj);
    float3 posR = ReconstructWorldPos(screenUV + float2( texelSize.x, 0.0f), dR, invViewProj);
    float3 posU = ReconstructWorldPos(screenUV + float2(0.0f, -texelSize.y), dU, invViewProj);
    float3 posD = ReconstructWorldPos(screenUV + float2(0.0f,  texelSize.y), dD, invViewProj);

    // 水平・垂直接線ベクトルの外積で法線を算出
    float3 tangentH = posR - posL;
    float3 tangentV = posD - posU;
    return normalize(cross(tangentH, tangentV));
}

/// 深度バッファ衝突の検出と応答
bool ResolveDepthCollision(
    inout float3 currentPos,
    inout float3 prevPos,
    float4x4 viewProj,
    float4x4 invViewProj,
    float2 screenSize,
    float3 cameraPos,
    float collisionRestitution,
    float particleRadius,
    float depthBias,
    Texture2D<float> depthTex,
    SamplerState depthSamp)
{
    // パーティクルをスクリーン空間に投影
    float3 screenData = ProjectToScreen(currentPos, viewProj);
    float2 screenUV = screenData.xy;
    float particleNDCDepth = screenData.z;

    // 画面外のパーティクルはスキップ
    if (screenUV.x < 0.0f || screenUV.x > 1.0f ||
        screenUV.y < 0.0f || screenUV.y > 1.0f)
    {
        return false;
    }

    // カメラ背面またはファー平面外のパーティクルはスキップ
    if (particleNDCDepth < 0.0f || particleNDCDepth > 1.0f)
    {
        return false;
    }

    // シーン深度をサンプル
    float sceneDepth = depthTex.SampleLevel(depthSamp, screenUV, 0);

    // 貫通検出: NDC 深度の比較
    if (particleNDCDepth <= sceneDepth)
    {
        return false;
    }

    // -------------------- 衝突検出 -------------------- //

    // シーン表面のワールド位置を復元
    float3 surfacePos = ReconstructWorldPos(screenUV, sceneDepth, invViewProj);

    // ワールド空間で距離チェック: 離れすぎは深度不連続による偽陽性
    // depthBias をワールド空間の最大許容距離として使用
    float worldDist = distance(currentPos, surfacePos);
    if (worldDist > depthBias)
    {
        return false;
    }

    float2 texelSize = 1.0f / screenSize;

    // 深度不連続チェック: シルエットエッジでの異常法線を回避
    float dL = depthTex.SampleLevel(depthSamp, screenUV + float2(-texelSize.x, 0.0f), 0);
    float dR = depthTex.SampleLevel(depthSamp, screenUV + float2( texelSize.x, 0.0f), 0);
    float dU = depthTex.SampleLevel(depthSamp, screenUV + float2(0.0f, -texelSize.y), 0);
    float dD = depthTex.SampleLevel(depthSamp, screenUV + float2(0.0f,  texelSize.y), 0);

    float maxDepthDiff = max(max(abs(dL - sceneDepth), abs(dR - sceneDepth)),
                             max(abs(dU - sceneDepth), abs(dD - sceneDepth)));
    if (maxDepthDiff > 0.1f)
    {
        return false;
    }

    float3 normal = EstimateDepthNormal(screenUV, texelSize, invViewProj, depthTex, depthSamp);

    // 法線がカメラ側を向くように保証
    float3 toCamera = normalize(cameraPos - currentPos);
    if (dot(normal, toCamera) < 0.0f)
    {
        normal = -normal;
    }

    //パーティクルを表面に押し出し
    float3 correctedPos = surfacePos + normal * particleRadius * 2;

    // Verlet 速度の反射
    float3 velocity = currentPos - prevPos;
    float3 reflectedVelocity = reflect(velocity, normal) * collisionRestitution;

    // 位置を更新
    currentPos = correctedPos;
    prevPos = correctedPos - reflectedVelocity;

    return true;
}

#endif // DEPTH_COLLISION_HLSLI
