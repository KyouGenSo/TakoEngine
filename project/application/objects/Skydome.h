#pragma once
#include "Object3d.h"
#include "Transform.h"
#include "Vector3.h"
#include <cassert>
#include "memory"

class Skydome {
public:
	Skydome();
	~Skydome();

	void Initialize(Object3d* model);
	void Update();
	void Draw();

private:
	Object3d* model_ = nullptr;
	Transform transform_;
};