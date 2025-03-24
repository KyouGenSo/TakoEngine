#include "GPUParticleEmitter.h"
#include "GPUParticle.h"

GPUParticleEmitter::GPUParticleEmitter(GPUParticle* particleSystem, uint32_t emitterId)
  : particleSystem_(particleSystem)
  , emitterId_(emitterId)
  , position_(Vector3(0.0f, 0.0f, 0.0f))
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
    EmitterData params;
    params.position = position_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetActive(bool isActive)
{
  isActive_ = isActive;

  if (particleSystem_) {
    EmitterData params;
    params.isActive = isActive_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetColor(const Vector4& color)
{
  color_ = color;

  if (particleSystem_) {
    EmitterData params;
    params.colorTint = color_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetParticleCount(uint32_t count)
{
  particleCount_ = count;

  if (particleSystem_) {
    EmitterData params;
    params.count = particleCount_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}

void GPUParticleEmitter::SetFrequency(float frequency)
{
  frequency_ = frequency;

  if (particleSystem_) {
    EmitterData params;
    params.frequency = frequency_;
    particleSystem_->UpdateEmitterParameters(emitterId_, params);
  }
}