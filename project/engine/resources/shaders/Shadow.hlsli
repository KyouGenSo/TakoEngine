#ifndef TAKO_SHADOW_HLSLI
#define TAKO_SHADOW_HLSLI

#include "Common.hlsli"

#define SHADOW_CASCADE_MAX 4
#define SHADOW_SLICE_MAX 20

// 影の区画 1 つ。カスケード・スポットライト・点光源の 1 面のどれも同じ形（ShadowRenderer::ShadowSlice と一致させること）
struct ShadowSlice
{
    float4x4 worldToAtlas; // ワールド → アトラスの UV と深度
    float4 uvBounds;       // 区画の UV の範囲（xy: 最小、zw: 最大）。隣の区画を読まないよう半テクセル内側
    float4 params;         // x: 1 テクセルのワールドでの大きさ（透視なら距離 1 での値）、y: 透視なら 1
};

// ShadowRenderer::ShadowConstants と一致させること
struct ShadowConstants
{
    ShadowSlice slices[SHADOW_SLICE_MAX];      // 0〜3: カスケード、4〜19: スポットライト・点光源
    float4 cascadeSpheres[SHADOW_CASCADE_MAX]; // xyz: 中心、w: 半径の 2 乗
    float3 cameraPosition;
    float maxDistance;
    float2 atlasTexel;      // gShadowMap の 1 テクセルの UV
    float2 localAtlasTexel; // gLocalShadowMap の 1 テクセルの UV
    int enableShadow;
    int cascadeCount;
    int pcfKernelSize;      // 1, 3, 5, 7
    int debugCascades;
    float depthBias;        // ライトの向きへずらす量（テクセル）
    float normalBias;       // 法線の向きへずらす量（テクセル）
    float2 padding;
};

ConstantBuffer<ShadowConstants> gShadowConstants : register(b4);
Texture2D<float> gShadowMap : register(t4);      // 平行光源のカスケード
Texture2D<float> gLocalShadowMap : register(t6); // スポットライト・点光源

float SampleShadowTap(Texture2D<float> map, float2 baseUv, float u, float v, float2 texel, float4 bounds, float depth)
{
    float2 uv = clamp(baseUv + float2(u, v) * texel, bounds.xy, bounds.zw);
    return map.SampleCmpLevelZero(gShadowCmp, uv, depth);
}

// テント型の重みの PCF。比較サンプラーが 2x2 をまとめて返すので、3x3 は 4 回、5x5 は 9 回、7x7 は 16 回で読める
// （Ignacio Castaño の Optimized PCF。重みの和は 16 / 144 / 2704）
float SampleShadowPCF(Texture2D<float> map, float2 uv, float depth, float2 texel, float4 bounds, int kernel)
{
    if (kernel < 3)
    {
        return map.SampleCmpLevelZero(gShadowCmp, clamp(uv, bounds.xy, bounds.zw), depth);
    }

    float2 texelPos = uv / texel;
    float2 baseTexel = floor(texelPos + 0.5);
    float s = texelPos.x + 0.5 - baseTexel.x;
    float t = texelPos.y + 0.5 - baseTexel.y;
    float2 baseUv = (baseTexel - 0.5) * texel;

    float sum = 0.0;
    if (kernel == 3)
    {
        float uw0 = 3.0 - 2.0 * s;
        float uw1 = 1.0 + 2.0 * s;
        float u0 = (2.0 - s) / uw0 - 1.0;
        float u1 = s / uw1 + 1.0;

        float vw0 = 3.0 - 2.0 * t;
        float vw1 = 1.0 + 2.0 * t;
        float v0 = (2.0 - t) / vw0 - 1.0;
        float v1 = t / vw1 + 1.0;

        sum += uw0 * vw0 * SampleShadowTap(map, baseUv, u0, v0, texel, bounds, depth);
        sum += uw1 * vw0 * SampleShadowTap(map, baseUv, u1, v0, texel, bounds, depth);
        sum += uw0 * vw1 * SampleShadowTap(map, baseUv, u0, v1, texel, bounds, depth);
        sum += uw1 * vw1 * SampleShadowTap(map, baseUv, u1, v1, texel, bounds, depth);
        return sum / 16.0;
    }
    if (kernel == 5)
    {
        float uw0 = 4.0 - 3.0 * s;
        float uw1 = 7.0;
        float uw2 = 1.0 + 3.0 * s;
        float u0 = (3.0 - 2.0 * s) / uw0 - 2.0;
        float u1 = (3.0 + s) / uw1;
        float u2 = s / uw2 + 2.0;

        float vw0 = 4.0 - 3.0 * t;
        float vw1 = 7.0;
        float vw2 = 1.0 + 3.0 * t;
        float v0 = (3.0 - 2.0 * t) / vw0 - 2.0;
        float v1 = (3.0 + t) / vw1;
        float v2 = t / vw2 + 2.0;

        sum += uw0 * vw0 * SampleShadowTap(map, baseUv, u0, v0, texel, bounds, depth);
        sum += uw1 * vw0 * SampleShadowTap(map, baseUv, u1, v0, texel, bounds, depth);
        sum += uw2 * vw0 * SampleShadowTap(map, baseUv, u2, v0, texel, bounds, depth);
        sum += uw0 * vw1 * SampleShadowTap(map, baseUv, u0, v1, texel, bounds, depth);
        sum += uw1 * vw1 * SampleShadowTap(map, baseUv, u1, v1, texel, bounds, depth);
        sum += uw2 * vw1 * SampleShadowTap(map, baseUv, u2, v1, texel, bounds, depth);
        sum += uw0 * vw2 * SampleShadowTap(map, baseUv, u0, v2, texel, bounds, depth);
        sum += uw1 * vw2 * SampleShadowTap(map, baseUv, u1, v2, texel, bounds, depth);
        sum += uw2 * vw2 * SampleShadowTap(map, baseUv, u2, v2, texel, bounds, depth);
        return sum / 144.0;
    }

    float uw0 = 5.0 * s - 6.0;
    float uw1 = 11.0 * s - 28.0;
    float uw2 = -(11.0 * s + 17.0);
    float uw3 = -(5.0 * s + 1.0);
    float u0 = (4.0 * s - 5.0) / uw0 - 3.0;
    float u1 = (4.0 * s - 16.0) / uw1 - 1.0;
    float u2 = -(7.0 * s + 5.0) / uw2 + 1.0;
    float u3 = -s / uw3 + 3.0;

    float vw0 = 5.0 * t - 6.0;
    float vw1 = 11.0 * t - 28.0;
    float vw2 = -(11.0 * t + 17.0);
    float vw3 = -(5.0 * t + 1.0);
    float v0 = (4.0 * t - 5.0) / vw0 - 3.0;
    float v1 = (4.0 * t - 16.0) / vw1 - 1.0;
    float v2 = -(7.0 * t + 5.0) / vw2 + 1.0;
    float v3 = -t / vw3 + 3.0;

    sum += uw0 * vw0 * SampleShadowTap(map, baseUv, u0, v0, texel, bounds, depth);
    sum += uw1 * vw0 * SampleShadowTap(map, baseUv, u1, v0, texel, bounds, depth);
    sum += uw2 * vw0 * SampleShadowTap(map, baseUv, u2, v0, texel, bounds, depth);
    sum += uw3 * vw0 * SampleShadowTap(map, baseUv, u3, v0, texel, bounds, depth);
    sum += uw0 * vw1 * SampleShadowTap(map, baseUv, u0, v1, texel, bounds, depth);
    sum += uw1 * vw1 * SampleShadowTap(map, baseUv, u1, v1, texel, bounds, depth);
    sum += uw2 * vw1 * SampleShadowTap(map, baseUv, u2, v1, texel, bounds, depth);
    sum += uw3 * vw1 * SampleShadowTap(map, baseUv, u3, v1, texel, bounds, depth);
    sum += uw0 * vw2 * SampleShadowTap(map, baseUv, u0, v2, texel, bounds, depth);
    sum += uw1 * vw2 * SampleShadowTap(map, baseUv, u1, v2, texel, bounds, depth);
    sum += uw2 * vw2 * SampleShadowTap(map, baseUv, u2, v2, texel, bounds, depth);
    sum += uw3 * vw2 * SampleShadowTap(map, baseUv, u3, v2, texel, bounds, depth);
    sum += uw0 * vw3 * SampleShadowTap(map, baseUv, u0, v3, texel, bounds, depth);
    sum += uw1 * vw3 * SampleShadowTap(map, baseUv, u1, v3, texel, bounds, depth);
    sum += uw2 * vw3 * SampleShadowTap(map, baseUv, u2, v3, texel, bounds, depth);
    sum += uw3 * vw3 * SampleShadowTap(map, baseUv, u3, v3, texel, bounds, depth);
    return sum / 2704.0;
}

// 区画 1 つから影の割合を読む（1 = 光が当たる、0 = 影）。区画の外は 1
// N: 面の法線、L: 面から光源への向き（どちらも正規化済み）、lightDistance: 光源までの距離（平行光源は使わない）
float SampleShadowSlice(Texture2D<float> map, float2 atlasTexel, ShadowSlice slice, float3 worldPos, float3 N, float3 L, float lightDistance)
{
    // 縞（アクネ）を避けるため、読む位置を法線と光源の向きへテクセルの大きさに比例してずらす
    float texelWorld = slice.params.x * lerp(1.0, lightDistance, slice.params.y);
    float3 biasedPos = worldPos + N * (gShadowConstants.normalBias * texelWorld) + L * (gShadowConstants.depthBias * texelWorld);

    float4 atlasPos = mul(float4(biasedPos, 1.0), slice.worldToAtlas);
    if (atlasPos.w <= 0.0)
    {
        return 1.0;
    }
    atlasPos.xyz /= atlasPos.w;
    if (any(atlasPos.xy < slice.uvBounds.xy) || any(atlasPos.xy > slice.uvBounds.zw) || atlasPos.z < 0.0 || atlasPos.z > 1.0)
    {
        return 1.0;
    }
    return SampleShadowPCF(map, atlasPos.xy, atlasPos.z, atlasTexel, slice.uvBounds, gShadowConstants.pcfKernelSize);
}

// worldPos が入る一番細かいカスケードの番号（どれにも入らなければ -1）。球で選ぶので、どの視点から描いても同じ結果になる
int ShadowCascadeIndex(float3 worldPos)
{
    for (int i = 0; i < gShadowConstants.cascadeCount; ++i)
    {
        float3 toCenter = worldPos - gShadowConstants.cascadeSpheres[i].xyz;
        if (dot(toCenter, toCenter) < gShadowConstants.cascadeSpheres[i].w)
        {
            return i;
        }
    }
    return -1;
}

// 平行光源の影の割合。lightDirection は光の進む向き
float DirectionalShadow(float3 worldPos, float3 normal, float3 lightDirection)
{
    if (gShadowConstants.enableShadow == 0)
    {
        return 1.0;
    }
    float3 N = normalize(normal);
    float3 L = normalize(-lightDirection);
    // 光に背を向けた面は陰になるだけなので、シャドウマップは読まない
    if (dot(N, L) <= 0.0)
    {
        return 1.0;
    }
    int cascade = ShadowCascadeIndex(worldPos);
    if (cascade < 0)
    {
        return 1.0;
    }
    float shadow = SampleShadowSlice(gShadowMap, gShadowConstants.atlasTexel, gShadowConstants.slices[cascade], worldPos, N, L, 1.0);

    // 影の距離の手前 10% で薄くして、端で急に切れないようにする
    float cameraDistance = length(worldPos - gShadowConstants.cameraPosition);
    float fade = saturate((gShadowConstants.maxDistance - cameraDistance) / (gShadowConstants.maxDistance * 0.1));
    return lerp(1.0, shadow, fade);
}

// スポットライトの影の割合。slice はライトの shadowSlice（負なら影なし）
float SpotShadow(int slice, float3 lightPosition, float3 worldPos, float3 normal)
{
    if (slice < 0 || gShadowConstants.enableShadow == 0)
    {
        return 1.0;
    }
    float3 toLight = lightPosition - worldPos;
    float lightDistance = length(toLight);
    return SampleShadowSlice(gLocalShadowMap, gShadowConstants.localAtlasTexel, gShadowConstants.slices[slice], worldPos, normalize(normal), toLight / max(lightDistance, 0.0001), lightDistance);
}

// 点光源の影の割合。firstSlice はライトの shadowSlice（負なら影なし）で、そこから +X -X +Y -Y +Z -Z の 6 面が続く
float PointShadow(int firstSlice, float3 lightPosition, float3 worldPos, float3 normal)
{
    if (firstSlice < 0 || gShadowConstants.enableShadow == 0)
    {
        return 1.0;
    }
    float3 fromLight = worldPos - lightPosition;
    float3 axis = abs(fromLight);
    int face;
    if (axis.x >= axis.y && axis.x >= axis.z)
    {
        face = fromLight.x >= 0.0 ? 0 : 1;
    }
    else if (axis.y >= axis.z)
    {
        face = fromLight.y >= 0.0 ? 2 : 3;
    }
    else
    {
        face = fromLight.z >= 0.0 ? 4 : 5;
    }
    float lightDistance = length(fromLight);
    return SampleShadowSlice(gLocalShadowMap, gShadowConstants.localAtlasTexel, gShadowConstants.slices[firstSlice + face], worldPos, normalize(normal), -fromLight / max(lightDistance, 0.0001), lightDistance);
}

// カスケードの色分け（Engine Settings の Show Cascades）。オフなら白
float3 ShadowCascadeDebugTint(float3 worldPos)
{
    if (gShadowConstants.debugCascades == 0 || gShadowConstants.enableShadow == 0)
    {
        return float3(1.0, 1.0, 1.0);
    }
    static const float3 kTints[SHADOW_CASCADE_MAX + 1] =
    {
        float3(1.0, 0.45, 0.45), float3(0.45, 1.0, 0.45), float3(0.45, 0.55, 1.0), float3(1.0, 1.0, 0.45), float3(1.0, 1.0, 1.0)
    };
    int cascade = ShadowCascadeIndex(worldPos);
    return kTints[cascade < 0 ? SHADOW_CASCADE_MAX : cascade];
}

#endif
