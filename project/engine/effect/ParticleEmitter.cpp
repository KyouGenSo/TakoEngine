#include "ParticleEmitter.h"
#include "Draw2D.h"

void ParticleEmitter::Update()
{
	frequencyTime_ += ParticleManager::GetInstance()->GetDeltaTime();
	if (frequency_ <= frequencyTime_)
	{
		Emit();
		frequencyTime_ -= frequency_;
	}
}

void ParticleEmitter::Emit()
{
	ParticleManager::GetInstance()->Emit(name_, transform_.translate, transform_.scale, velocity_, range_, count_, color_, lifeTime_, isRandomColor_);
}

void ParticleEmitter::Draw()
{
	if (isVisualize_)
	{
		AABB emitterAABB;
		emitterAABB.min = range_.min + transform_.translate;
		emitterAABB.max = range_.max + transform_.translate;
		Draw2D::GetInstance()->DrawAABB(emitterAABB, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
	}
}
