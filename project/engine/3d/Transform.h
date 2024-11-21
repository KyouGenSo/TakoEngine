#pragma once
#include "Vector3.h"
#include "Quaternion.h"

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct QuatTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};