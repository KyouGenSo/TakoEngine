#include "TriangleEmitter.h"
#include "GPUParticle.h"

TriangleEmitter::TriangleEmitter(GPUParticle* particleSystem, const Vector3& position,
  const Vector3& v1, const Vector3& v2, const Vector3& v3,
  uint32_t count, float frequency)
  : GPUParticleEmitter(particleSystem, 0) // 一時的なIDを設定
{
  // パラメータの初期化
  SetPosition(position);
  SetVertices(v1, v2, v3);
  SetParticleCount(count);
  SetFrequency(frequency);

  // エミッターのタイプを設定
  data_.type = EmitterType::Triangle;

}

std::shared_ptr<GPUParticleEmitter> TriangleEmitter::Clone() const
{
  // 新しいインスタンスを作成し、現在のデータをコピー
  auto clone = std::make_shared<TriangleEmitter>(
    particleSystem_, GetPosition(), GetVertex1(), GetVertex2(), GetVertex3(), GetParticleCount(), GetFrequency());

  // 他のプロパティも転送
  clone->SetColors(GetStartColor(), GetEndColor());
  clone->SetVelRange(GetVelRangeX(), GetVelRangeY(), GetVelRangeZ());
  clone->SetLifeTimeRange(GetLifeTimeRange());
  clone->SetScaleRange(GetScaleRangeX(), GetScaleRangeY());
  clone->SetActive(IsActive());
  clone->SetNormalize(IsNormalize());
  clone->SetRandomRotateZ(IsRandomRotateZ());
  clone->SetFrequencyTime(GetFrequency());

  return clone;
}

void TriangleEmitter::SetupGPUData(EmitterGPUData& gpuData) const
{
  GPUParticleEmitter::SetupGPUData(gpuData);

  // 三角形固有のデータを設定
  gpuData.triangleV1 = data_.triangle.v1;
  gpuData.triangleV2 = data_.triangle.v2;
  gpuData.triangleV3 = data_.triangle.v3;
  gpuData.type = static_cast<uint32_t>(data_.type);
}

void TriangleEmitter::SetVertices(const Vector3& v1, const Vector3& v2, const Vector3& v3)
{
  data_.triangle.v1 = v1;
  data_.triangle.v2 = v2;
  data_.triangle.v3 = v3;
}
