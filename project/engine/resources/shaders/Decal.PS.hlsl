/// ============================================ ///
/// Decal.PS.hlsl - Projective Decal ピクセルシェーダー
/// 深度バッファからワールド座標を復元し、形状マスクを適用
/// ============================================ ///

#include "Decal.hlsli"

ConstantBuffer<ViewData> gViewData : register(b0);
ConstantBuffer<DecalData> gDecalData : register(b1);

// 深度テクスチャ（t0）
Texture2D<float> gDepthTexture : register(t0);
SamplerState gPointSampler : register(s0);

// デカールテクスチャ（t1、テクスチャモード時のみ使用）
Texture2D<float4> gDecalTexture : register(t1);
SamplerState gLinearSampler : register(s1);

/// 円形マスク
float CircleMask(float2 xz, float softness) {
    float dist = length(xz) * 2.0; // [-0.5,0.5] を [0,1] にスケール
    return 1.0 - smoothstep(1.0 - softness, 1.0, dist);
}

/// 扇形マスク
float FanMask(float2 xz, float halfAngle, float softness) {
    float dist = length(xz) * 2.0;
    float distMask = 1.0 - smoothstep(1.0 - softness, 1.0, dist);

    // z+ を前方として角度を計算
    float angle = abs(atan2(xz.x, xz.y));
    float angleMask = 1.0 - smoothstep(halfAngle - softness * 0.5, halfAngle, angle);

    return distMask * angleMask;
}

/// 矩形マスク
float RectangleMask(float2 xz, float softness) {
    float2 d = 0.5 - abs(xz);
    float maskX = smoothstep(0.0, softness, d.x);
    float maskY = smoothstep(0.0, softness, d.y);
    return maskX * maskY;
}

struct PixelOutput {
    float4 color : SV_TARGET;
};

PixelOutput main(VertexShaderOutput input) {
    PixelOutput output;

    // 1. SV_POSITION からスクリーン UV を算出
    float2 screenUV = input.pos.xy / float2(gViewData.screenWidth, gViewData.screenHeight);

    // 2. 深度テクスチャをサンプリング
    float depth = gDepthTexture.Sample(gPointSampler, screenUV);

    // 深度が最大（何も描画されていない領域）なら破棄
    if (depth >= 1.0) {
        discard;
    }

    // 3. NDC を再構築（DirectX: x[-1,1], y[-1,1], z[0,1]）
    float4 ndc;
    ndc.x = screenUV.x * 2.0 - 1.0;
    ndc.y = -(screenUV.y * 2.0 - 1.0); // Y 反転
    ndc.z = depth;
    ndc.w = 1.0;

    // 4. invViewProj でワールド座標を復元
    float4 worldPos = mul(ndc, gViewData.invViewProj);
    worldPos /= worldPos.w;

    // 5. デカールローカル座標に変換
    float4 localPos = mul(worldPos, gDecalData.decalWorldInverse);

    // [-0.5, 0.5] 範囲外なら破棄
    clip(0.5 - abs(localPos.x));
    clip(0.5 - abs(localPos.y));
    clip(0.5 - abs(localPos.z));

    // 6. 形状マスクを計算
    float shapeMask = 0.0;
    float2 xz = localPos.xz;

    if (gDecalData.useTexture == 1) {
        // テクスチャモード: localPos.xz + 0.5 で [0,1] UV に変換
        float2 uv = xz + 0.5;
        float4 texColor = gDecalTexture.Sample(gLinearSampler, uv);
        output.color = texColor * gDecalData.color;
        output.color.a *= texColor.a;
        return output;
    }

    // プロシージャルモード
    if (gDecalData.shapeType == 0) {
        // Circle
        shapeMask = CircleMask(xz, gDecalData.edgeSoftness);
    } else if (gDecalData.shapeType == 1) {
        // Fan
        shapeMask = FanMask(xz, gDecalData.fanHalfAngle, gDecalData.edgeSoftness);
    } else {
        // Rectangle
        shapeMask = RectangleMask(xz, gDecalData.edgeSoftness);
    }

    // 7. 出力カラーを計算
    output.color = gDecalData.color;
    output.color.a *= shapeMask;

    // マスクがほぼ 0 なら破棄（パフォーマンス最適化）
    if (output.color.a < 0.001) {
        discard;
    }

    return output;
}
