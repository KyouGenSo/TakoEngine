#include "GPUParticleEmitter.h"
#include "GPUParticle.h"

namespace Tako {

GPUParticleEmitter::GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId)
  : particleSystem_(particleSystem)
{
  data_.emitterID = emitterId;
  data_.isActive = true;
  data_.isEmitting = false;
  data_.isNormalize = false;
  data_.isRandomRotateZ = false;
  data_.isTemp = false;
  data_.emitterLifeTime = 0.0f;
  data_.emitterCurrentTime = 0.0f;
  data_.frequencyTime = 0.0f;
  data_.position = Vector3(0.0f, 0.0f, 0.0f);
  data_.scaleRangeX = Vector2(0.0f, 0.0f);
  data_.scaleRangeY = Vector2(0.0f, 0.0f);
  data_.velRangeX = Vector2(0.0f, 0.0f);
  data_.velRangeY = Vector2(0.0f, 0.0f);
  data_.velRangeZ = Vector2(0.0f, 0.0f);
  data_.lifeTimeRange = Vector2(0.0f, 0.0f);
  data_.startColorTint = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  data_.endColorTint = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  data_.count = 0;
  data_.frequency = 0.0f;
}

GPUParticleEmitter::~GPUParticleEmitter()
{
  // デストラクタで特に何もしない
  // 必要なリソースはGPUParticleクラスが管理しているため
  // ここで解放する必要はない
}

void GPUParticleEmitter::SetupGPUData(EmitterGPUData& gpuData) const
{
  gpuData.type = static_cast<uint32_t>(data_.type);
  gpuData.isActive = data_.isActive ? 1u : 0u;
  gpuData.isEmit = data_.isEmitting ? 1u : 0u;
  gpuData.isNormalize = data_.isNormalize ? 1u : 0u;
  gpuData.isRandomRotateZ = data_.isRandomRotateZ ? 1u : 0u;
  gpuData.emitterID = data_.emitterID;

  gpuData.position = data_.position;
  gpuData.scaleRangeX = data_.scaleRangeX;
  gpuData.scaleRangeY = data_.scaleRangeY;
  gpuData.velRangeX = data_.velRangeX;
  gpuData.velRangeY = data_.velRangeY;
  gpuData.velRangeZ = data_.velRangeZ;
  gpuData.lifeTimeRange = data_.lifeTimeRange;

  gpuData.startColorTint = data_.startColorTint;
  gpuData.endColorTint = data_.endColorTint;

  gpuData.count = data_.count;
  gpuData.frequency = data_.frequency;
  gpuData.frequencyTime = data_.frequencyTime;

  // 一時的なエミッター用のデータをコピー
  gpuData.isTemp = data_.isTemp ? 1u : 0u;
  gpuData.emitterLifeTime = data_.emitterLifeTime;
  gpuData.emitterCurrentTime = data_.emitterCurrentTime;

  // 型固有のデータをコピー
  switch (data_.type) {
  case EmitterType::Sphere:
    gpuData.radius = data_.sphere.radius;
    break;
  case EmitterType::Box:
    gpuData.boxSize = data_.box.size;
    gpuData.boxRotation = data_.box.rotation;
    break;
  case EmitterType::Triangle:
    gpuData.triangleV1 = data_.triangle.v1;
    gpuData.triangleV2 = data_.triangle.v2;
    gpuData.triangleV3 = data_.triangle.v3;
    break;
  }
}

void GPUParticleEmitter::UpdateEmission(float deltaTime)
{
  // 非アクティブならスキップ
  if (!data_.isActive) {
    data_.isEmitting = false;
    return;
  }

  // 射出タイマーを更新
  data_.frequencyTime += deltaTime;

  // 射出間隔を超えたら射出許可を出して時間を調整
  if(data_.frequency <= data_.frequencyTime) {
    data_.isEmitting = true;

    // 余剰時間を調整（蓄積誤差を防ぐ）
    data_.frequencyTime = fmodf(data_.frequencyTime, data_.frequency);
  } else {
    data_.isEmitting = false;
  }
}

void GPUParticleEmitter::SetPosition(const Vector3& position)
{
  data_.position = position;
}

void GPUParticleEmitter::SetActive(bool isActive)
{
  data_.isActive = isActive;
}

void GPUParticleEmitter::SetEmitting(bool cond)
{
  data_.isEmitting = cond;
}

void GPUParticleEmitter::SetNormalize(bool isNormalize)
{
  data_.isNormalize = isNormalize;
}

void GPUParticleEmitter::SetRandomRotateZ(bool isRandomRotateZ)
{
  data_.isRandomRotateZ = isRandomRotateZ;
}

void GPUParticleEmitter::SetColor(const Vector4& color)
{
  SetColors(color, color);
}

void GPUParticleEmitter::SetStartColor(const Vector4& color)
{
  data_.startColorTint = color;
}

void GPUParticleEmitter::SetEndColor(const Vector4& color)
{
  data_.endColorTint = color;
}

void GPUParticleEmitter::SetColors(const Vector4& startColor, const Vector4& endColor)
{
  data_.startColorTint = startColor;
  data_.endColorTint = endColor;
}

void GPUParticleEmitter::SetParticleCount(uint32_t count)
{
  data_.count = count;
}

void GPUParticleEmitter::SetFrequency(float frequency)
{
  data_.frequency = frequency;
}

void GPUParticleEmitter::SetFrequencyTime(float frequencyTime)
{
  data_.frequencyTime = frequencyTime;
}

void GPUParticleEmitter::SetScaleRange(const Vector2& rangeX, const Vector2& rangeY)
{
  SetScaleRangeX(rangeX);
  SetScaleRangeY(rangeY);
}

void GPUParticleEmitter::SetScaleRangeX(const Vector2& range)
{
  data_.scaleRangeX = range;
}

void GPUParticleEmitter::SetScaleRangeY(const Vector2& range)
{
  data_.scaleRangeY = range;
}

void GPUParticleEmitter::SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ)
{
  SetVelRangeX(rangeX);
  SetVelRangeY(rangeY);
  SetVelRangeZ(rangeZ);
}

void GPUParticleEmitter::SetVelRangeX(const Vector2& range)
{
  data_.velRangeX = range;
}

void GPUParticleEmitter::SetVelRangeY(const Vector2& range)
{
  data_.velRangeY = range;
}

void GPUParticleEmitter::SetVelRangeZ(const Vector2& range)
{
  data_.velRangeZ = range;
}

void GPUParticleEmitter::SetLifeTimeRange(const Vector2& range)
{
  data_.lifeTimeRange = range;
}

void GPUParticleEmitter::SetTemporary(bool isTemporary, float lifeTime)
{
  data_.isTemp = isTemporary;
  data_.emitterLifeTime = lifeTime;
  data_.emitterCurrentTime = 0.0f;
}

void GPUParticleEmitter::UpdateTemporaryLifeTime(const float deltaTime)
{
  if (!data_.isTemp || data_.emitterLifeTime <= 0.0f) return;

  data_.emitterCurrentTime += deltaTime;
}

bool GPUParticleEmitter::IsLifeTimeExpired() const
{
  if (!data_.isTemp || data_.emitterLifeTime <= 0.0f) return false;
  return data_.emitterCurrentTime >= data_.emitterLifeTime;
}

} // namespace Tako
