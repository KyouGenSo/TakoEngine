#pragma once
#include "Vector3.h"

// sphereEmitterの設定
struct SphereEmitterParams
{
  Vector3 position;
  float radius;
  uint32_t count;
  float frequency;
};

// boxEmitterの設定
struct BoxEmitterParams
{
  Vector3 position;
  Vector3 size;
  Vector3 rotation;
  uint32_t count;
  float frequency;
};

// triangleEmitterの設定
struct TriangleEmitterParams
{
  Vector3 position;
  Vector3 v1;
  Vector3 v2;
  Vector3 v3;
  uint32_t count;
  float frequency;
};