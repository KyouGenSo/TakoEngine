#include "Particle.hlsli"

// ���\�[�X�o�C���f�B���O
StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

// ���_�V�F�[�_�[����
struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 texcoord : TEXCOORD0;
};

// ���_�V�F�[�_�[���C���֐�
VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // �C���X�^���XID�ɑΉ�����p�[�e�B�N���f�[�^���擾
    Particle particle = gParticles[instanceID];
    
    // �r���{�[�h�s����擾
    float4x4 worldMat = gPerView.billboardMat;

	// Z����]��K�p----------------------------------------
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
    
    // �p�[�e�B�N���̈ʒu��K�p
    worldMat[3].xyz = particle.translate;
    
    // ���_�ʒu�̌v�Z
    output.pos = mul(input.pos, mul(worldMat, gPerView.viewProj));
    
    // �e�N�X�`�����W�����̂܂܏o��
    output.texcoord = input.texcoord;
    
    // �F�����o��
    // �����Ɋ�Â��ĐF����`���
    float lifeRatio = particle.currentTime / particle.lifeTime;
    output.color = lerp(particle.startColor, particle.endColor, lifeRatio);
    
    return output;
}