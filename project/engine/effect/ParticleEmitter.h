#pragma once
#include "ParticleManager.h"

class ParticleEmitter {
public: // メンバー関数

	ParticleEmitter(std::string name, Transform transform, Vector3 velocity, AABB range, float lifeTime, uint32_t count,Vector4 color, float frequency, bool isRandomColor)
		: name_(name), transform_(transform), velocity_(velocity), range_(range), lifeTime_(lifeTime), count_(count), color_(color), frequency_(frequency), isRandomColor_(isRandomColor)
	{
		frequencyTime_ = 0.0f;
		isVisualize_ = false;
	}

	void Update();

	void Emit();

	void Draw();

	// -----------------------------------Getters-----------------------------------//
	

	// -----------------------------------Setters-----------------------------------//
	void SetTranslate(Vector3 translate) { transform_.translate = translate; }
	void SetScale(Vector3 scale) { transform_.scale = scale; }
	void SetFrequency(float frequency) { frequency_ = frequency; }
	void SetCount(uint32_t count) { count_ = count; }
	void SetVelocity(Vector3 velocity) { velocity_ = velocity; }
	void SetRange(AABB range) { range_ = range; }
	void SetColor(Vector4 color) { color_ = color; }
	void SetLifeTime(float lifeTime) { lifeTime_ = lifeTime; }
	void SetIsRandomColor(bool isRandomColor) { isRandomColor_ = isRandomColor; }
	void SetIsVisualize(bool isVisualize) { isVisualize_ = isVisualize; }

private: // メンバー変数

	std::string name_;
	Transform transform_;
	Vector3 velocity_;
	AABB range_;
	float lifeTime_;
	uint32_t count_;
	Vector4 color_;
	float frequency_;
	float frequencyTime_;
	bool isRandomColor_;

	bool isVisualize_ = false;
};