#pragma once
#include "ParticleManager.h"

class ParticleEmitter {
public: // メンバー関数

	ParticleEmitter(std::string name, Transform transform, uint32_t count, float frequency, bool isRandomColor)
	{
		name_ = name;
		transform_ = transform;
		count_ = count;
		frequency_ = frequency;
		frequencyTime_ = 0.0f;
		isRandomColor_ = isRandomColor;
	}

	void Emit();

	// -----------------------------------Getters-----------------------------------//
	

	// -----------------------------------Setters-----------------------------------//
	void SetTranslate(Vector3 translate) { transform_.translate = translate; }
	void SetFrequency(float frequency) { frequency_ = frequency; }
	void SetCount(uint32_t count) { count_ = count; }


private: // メンバー変数

	std::string name_;
	Transform transform_;
	uint32_t count_;
	float frequency_;
	float frequencyTime_;
	bool isRandomColor_;

};