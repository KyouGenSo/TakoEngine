#pragma once
#include "Object3d.h"
#include "Vector3.h"
#include "Transform.h"
#include "memory"
#include <cassert>

class Ground {
public:
	Ground();
	~Ground();

	void Initialize(Object3d* model);
	void Update();
	void Draw();

private:
	Object3d* model_ = nullptr;
	Transform transform_;
};