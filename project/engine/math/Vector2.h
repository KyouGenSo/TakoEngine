#pragma once
#include <cmath>

struct Vector2 {

	float x, y;

  float Length() const {
    return std::sqrt(x * x + y * y);
  }
};