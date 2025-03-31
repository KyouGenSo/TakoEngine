#include "GPUParticleEmitter.h"
#include "GPUParticle.h"

GPUParticleEmitter::GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId)
  : particleSystem_(particleSystem)
  , emitterId_(emitterId)
  , position_(Vector3(0.0f, 0.0f, 0.0f))
  , scaleRangeX_(Vector2(0.0f, 0.0f))
  , scaleRangeY_(Vector2(0.0f, 0.0f))
  , velRangeX_(Vector2(0.0f, 0.0f))
  , velRangeY_(Vector2(0.0f, 0.0f))
  , velRangeZ_(Vector2(0.0f, 0.0f))
  , lifeTimeRange_(Vector2(0.0f, 0.0f))
  , color_(Vector4(1.0f, 1.0f, 1.0f, 1.0f))
  , particleCount_(20)
  , frequency_(0.5f)
  , isActive_(true)
{
}

GPUParticleEmitter::~GPUParticleEmitter()
{
  // エミッターがまだ有効ならシステムから削除
  if (particleSystem_) {
    particleSystem_->RemoveEmitterById(emitterId_);
  }
}

void GPUParticleEmitter::SetPosition(const Vector3& position)
{
  position_ = position;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.position = position_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetActive(bool isActive)
{
  isActive_ = isActive;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.isActive = isActive_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetColor(const Vector4& color)
{
  color_ = color;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.colorTint = color_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetParticleCount(uint32_t count)
{
  particleCount_ = count;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.count = particleCount_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetFrequency(float frequency)
{
  frequency_ = frequency;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.frequency = frequency_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetScaleRange(const Vector2& rangeX, const Vector2& rangeY)
{
  SetScaleRangeX(rangeX);
  SetScaleRangeY(rangeY);
}

void GPUParticleEmitter::SetScaleRangeX(const Vector2& range)
{
  scaleRangeX_ = range;
  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.scaleRangeX = scaleRangeX_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetScaleRangeY(const Vector2& range)
{
  scaleRangeY_ = range;
  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.scaleRangeY = scaleRangeY_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetVelRange(const Vector2& rangeX, const Vector2& rangeY, const Vector2& rangeZ)
{
  SetVelRangeX(rangeX);
  SetVelRangeY(rangeY);
  SetVelRangeZ(rangeZ);
}

void GPUParticleEmitter::SetVelRangeX(const Vector2& range)
{
  velRangeX_ = range;
  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.velRangeX = velRangeX_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetVelRangeY(const Vector2& range)
{
  velRangeY_ = range;
  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.velRangeY = velRangeY_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetVelRangeZ(const Vector2& range)
{
  velRangeZ_ = range;
  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.velRangeZ = velRangeZ_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetLifeTimeRange(const Vector2& range)
{
  lifeTimeRange_ = range;
  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.lifeTimeRange = lifeTimeRange_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetTemporary(bool isTemporary, float lifeTime)
{
  isTemp_ = isTemporary;
  emitterLifeTime_ = lifeTime;
  emitterCurrentTime_ = 0.0f;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.isTemp = isTemporary;
    params.emitterLifeTime = lifeTime;
    params.emitterCurrentTime = 0.0f;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::UpdateTemporaryLifeTime(const float deltaTime)
{
  if (!isTemp_ || emitterLifeTime_ <= 0.0f) return;

  emitterCurrentTime_ += deltaTime;

  if (particleSystem_) {
    EmitterData params = particleSystem_->GetEmitterData(emitterId_);
    params.emitterCurrentTime = emitterCurrentTime_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

bool GPUParticleEmitter::IsLifeTimeExpired() const
{
  if (!isTemp_ || emitterLifeTime_ <= 0.0f) return false;
  return emitterCurrentTime_ >= emitterLifeTime_;
}
