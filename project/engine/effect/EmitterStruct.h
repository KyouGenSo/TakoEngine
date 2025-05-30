#pragma once
#include "Vector3.h"

// sphereEmitterの設定
struct SphereEmitterParams
{
  Vector3 position;
  float radius;
  uint32_t count;
  float frequency;

  Vector2 scaleRangeX = { .x = 0.0f, .y = 0.0f };
  Vector2 scaleRangeY = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeX = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeY = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeZ = { .x = 0.0f, .y = 0.0f };
  Vector2 lifeTimeRange = { .x = 0.0f, .y = 0.0f };

  Vector4 startColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };
  Vector4 endColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };

  bool isActive = true;
  bool isNormalize = false;
  bool isRandomRotateZ = false;
};

// boxEmitterの設定
struct BoxEmitterParams
{
  Vector3 position;
  Vector3 size;
  Vector3 rotation;
  uint32_t count;
  float frequency;

  Vector2 scaleRangeX = { .x = 0.0f, .y = 0.0f };
  Vector2 scaleRangeY = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeX = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeY = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeZ = { .x = 0.0f, .y = 0.0f };
  Vector2 lifeTimeRange = { .x = 0.0f, .y = 0.0f };

  Vector4 startColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };
  Vector4 endColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };

  bool isActive = true;
  bool isNormalize;
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

  Vector2 scaleRangeX = { .x = 0.0f, .y = 0.0f };
  Vector2 scaleRangeY = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeX = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeY = { .x = 0.0f, .y = 0.0f };
  Vector2 velRangeZ = { .x = 0.0f, .y = 0.0f };
  Vector2 lifeTimeRange = { .x = 0.0f, .y = 0.0f };

  Vector4 startColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };
  Vector4 endColor = { .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f };

  bool isActive = true;
  bool isNormalize;
};