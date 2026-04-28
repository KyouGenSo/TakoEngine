#ifndef FORCE_FIELD_HLSLI
#define FORCE_FIELD_HLSLI

// フォースフィールドタイプ定義
#define FORCE_GRAVITY     0  // 方向重力
#define FORCE_DIRECTIONAL 1  // 方向風（方向 + 減衰）
#define FORCE_VORTEX      2  // 渦（回転軸 + 強度 + 半径減衰）
#define FORCE_ATTRACT     3  // 吸引
#define FORCE_REPEL       4  // 反発

// フォースフィールド構造体
struct ForceField
{
    uint   type;       // フォースタイプ
    float3 position;   // フォースの中心位置
    float3 direction;  // 方向（Gravity, Directional で使用）
    float  strength;   // 強度
    float  radius;     // 影響半径（0 = 無限）
    float  falloff;    // 減衰指数（distance^falloff）
    float2 pad;        // 16バイトアライメント用パディング
};

/// <summary>
/// フォースフィールドの力を評価する
/// パーティクルの位置に基づいて、フォースフィールドが与える加速度を計算
/// </summary>
/// <param name="field">評価するフォースフィールド</param>
/// <param name="particlePos">パーティクルの位置</param>
/// <returns>パーティクルに加わる加速度ベクトル</returns>
float3 EvaluateForceField(ForceField field, float3 particlePos)
{
    // フォース中心からパーティクルへのベクトル
    float3 toParticle = particlePos - field.position;
    float dist = length(toParticle);

    // 影響半径外なら力を適用しない（radius == 0 は無限範囲）
    if (field.radius > 0.0f && dist > field.radius)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // 距離減衰の計算（0除算回避）
    float attenuation = 1.0f;
    if (field.falloff > 0.0f && dist > 0.001f)
    {
        // radius > 0 の場合は正規化距離で減衰
        if (field.radius > 0.0f)
        {
            float normalizedDist = dist / field.radius;
            attenuation = pow(1.0f - saturate(normalizedDist), field.falloff);
        }
        else
        {
            attenuation = 1.0f / pow(max(dist, 0.001f), field.falloff);
        }
    }

    float3 force = float3(0.0f, 0.0f, 0.0f);

    switch (field.type)
    {
        case FORCE_GRAVITY:
        {
            // 方向重力: direction方向に一定の力
            force = field.direction * field.strength;
            break;
        }
        case FORCE_DIRECTIONAL:
        {
            // 方向風: direction方向に減衰付きの力
            force = field.direction * field.strength * attenuation;
            break;
        }
        case FORCE_VORTEX:
        {
            // 渦: direction を回転軸として回転力を生成
            // 回転軸に直交する方向の成分を取得し、90度回転
            float3 axis = normalize(field.direction);
            float3 projected = toParticle - dot(toParticle, axis) * axis;
            float projLen = length(projected);
            if (projLen > 0.001f)
            {
                // 接線方向 = 回転軸 × 半径方向
                float3 tangent = cross(axis, projected / projLen);
                force = tangent * field.strength * attenuation;
            }
            break;
        }
        case FORCE_ATTRACT:
        {
            // 吸引: フォース中心に向かう力
            if (dist > 0.001f)
            {
                force = -normalize(toParticle) * field.strength * attenuation;
            }
            break;
        }
        case FORCE_REPEL:
        {
            // 反発: フォース中心から離れる力
            if (dist > 0.001f)
            {
                force = normalize(toParticle) * field.strength * attenuation;
            }
            break;
        }
    }

    return force;
}

#endif // FORCE_FIELD_HLSLI
