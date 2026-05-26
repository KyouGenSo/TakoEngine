#include "Particle.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    
    Particle particle = gParticles[instanceID];
    
    float4x4 worldMat = gPerView.billboardMat;

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