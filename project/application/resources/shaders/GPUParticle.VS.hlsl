#include "Particle.hlsli"

// リソースバインディング
StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

// 頂点シェーダー入力
struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
};

// 頂点シェーダーメイン関数
VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // インスタンスIDに対応するパーティクルデータを取得
    Particle particle = gParticles[instanceID];
    
    // ビルボード行列を取得
    float4x4 worldMat = gPerView.billboardMat;

	// Z軸回転を適用----------------------------------------
    if (particle.rotate.z != 0.0f)
    {
        float s, c;
        sincos(particle.rotate.z, s, c);

        float3 right = worldMat[0].xyz;
        float3 up = worldMat[1].xyz;

        worldMat[0].xyz = right * c - up * s;
        worldMat[1].xyz = right * s + up * c;
    }
    
    // パーティクルのスケールを適用
    worldMat[0] *= particle.scale.x;
    worldMat[1] *= particle.scale.y;
    worldMat[2] *= particle.scale.z;
    
    // パーティクルの位置を適用
    worldMat[3].xyz = particle.translate;
    
    // 頂点位置の計算
    output.pos = mul(input.pos, mul(worldMat, gPerView.viewProj));
    
    // テクスチャ座標をそのまま出力
    output.texcoord = input.texcoord;
    
    // 色情報を出力
    // 寿命に基づいて色を線形補間
    float lifeRatio = particle.currentTime / particle.lifeTime;
    output.color = lerp(particle.startColor, particle.endColor, lifeRatio);
    
    return output;
}