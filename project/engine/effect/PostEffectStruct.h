#pragma once
#include <cstdint>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "vector3.h"
#include "Vector4.h"

// レンダーターゲット構造体
struct RenderTexture {
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
  uint32_t srvIndex;
};

struct VignetteParam
{
  float power;
  float range;
  float padding[2];
  Vector3 color;
  float padding2;
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

struct DepthOutlineParam
{
  Matrix4x4 projectionInverse;
  float outlineThickness;
};

struct DissolveParam
{
  float threshold; // 溶解のしきい値
  float edgeThickness;
  float padding[2]; // パディング追加
  Vector4 edgeColor;
};

struct WhiteNoiseParam
{
  float time;
};

using EffectParam = std::variant<
  VignetteParam,
  BloomParam,
  NewBloomParam,
  FogParam,
  RadialBlurParam,
  BWFilterParam,
  RGBSplitParam,
  LuminanceOutlineParam,
  DepthOutlineParam,
  CameraForGPU,
  DissolveParam,
  WhiteNoiseParam
>;