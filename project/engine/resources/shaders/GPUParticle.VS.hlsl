#include "Particle.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<Emitter> gEmitters : register(t2); // ビルボード ON/OFF フラグ参照用
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
    // per-instance 頂点ストリーム (slot1): コンパクション済みパーティクル index。
    // ExecuteIndirect の StartInstanceLocation=base_e により、IA が
    // drawIndexList[base_e + SV_InstanceID] を自動的にフェッチして渡す。
    uint particleIndex : TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    Particle particle = gParticles[input.particleIndex];

    // エミッターのフラグでビルボード ON/OFF を判定する
    bool useBillboard = true;
    uint eid = particle.emitterId;
    if (eid < kMaxEmitters)
    {
        useBillboard = (gEmitters[eid].flags & EFLAG_BILLBOARD) != 0;
    }

    // ビルボード時はカメラ追従行列、非ビルボード時は単位行列(ワールド固定向き)を基底にする。
    // 以降の Z 回転 / スケール / 平行移動は両者で共通。
    float4x4 identityMat = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    float4x4 worldMat = useBillboard ? gPerView.billboardMat : identityMat;

	// パーティクルの回転を適用 (Z軸回転のみ)
    if (particle.rotate.z != 0.0f)
    {
        float s, c;
        sincos(particle.rotate.z, s, c);

        float3 right = worldMat[0].xyz;
        float3 up = worldMat[1].xyz;

        worldMat[0].xyz = right * c - up * s;
        worldMat[1].xyz = right * s + up * c;
    }
    
    // パーティクルのスケールを適用 (SCALE_FADE フラグがあれば endScale へ補間)
    float lifeRatioForScale = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
    float3 currentScale = (particle.flags & PFLAG_SCALE_FADE)
        ? lerp(particle.scale, particle.endScale, lifeRatioForScale)
        : particle.scale;
    worldMat[0] *= currentScale.x;
    worldMat[1] *= currentScale.y;
    worldMat[2] *= currentScale.z;
    
    // パーティクルの位置を適用
    worldMat[3].xyz = particle.translate;
    
    // ワールドビュー射影行列を乗算して最終的な頂点位置を計算
    output.pos = mul(input.pos, mul(worldMat, gPerView.viewProj));
    
    // テクスチャ座標はそのまま渡す
    output.texcoord = input.texcoord;
    
    // パーティクルの色を計算
    float lifeRatio = particle.currentTime / particle.lifeTime;
    output.color = lerp(particle.startColor, particle.endColor, lifeRatio);
    
    return output;
}