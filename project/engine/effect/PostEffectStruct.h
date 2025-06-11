#pragma once
#include <cstdint>

#include "Vector2.h"
#include "Vector4.h"

struct VignetteParam
{
  float power;
  float range;
};

struct VignetteRedBloomParam
{
  float power;
  float range;
  float threshold;
};

struct BloomParam
{
  float intensity;
  float threshold;
  float sigma;
  int kernelSize;
  Vector2 direction;
  int padding1;        // パディング追加
  int padding2;        // パディング追加
};

struct NewBloomParam
{
  float intensity;
  float threshold;
  float sigma;
  Vector2 direction;
  Vector2 texelSize;
  int sampleCount;
  int iteration;
  int padding3;        // パディング追加
  int padding4;        // パディング追加
};

struct PixelateParam
{
  float pixelSize;
};

// Shader用のカメラ
struct CameraForGPU
{
  float nearPlane;
  float farPlane;
};

struct FogParam
{
  Vector4 color;
  float density;
};

struct RadialBlurParam
{
  Vector2 center;
  float blurWidth;
  int32_t sampleCount;
};

struct BWFilterParam
{
  float threshold;
};

struct RGBSplitParam
{
  Vector2 redOffset;   // Rチャンネルのオフセット
  Vector2 greenOffset; // Gチャンネルのオフセット
  Vector2 blueOffset;  // Bチャンネルのオフセット
  float intensity;     // エフェクトの強度
};

struct LuminanceOutlineParam
{
  float outlineThickness;
};