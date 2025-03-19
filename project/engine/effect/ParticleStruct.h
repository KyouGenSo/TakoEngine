#pragma once
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"
#include "Transform.h"
#include "AABB.h"
#include "ModelStruct.h"

// マテリアル
struct ParticleMaterial
{
  Vector4 color;
  Matrix4x4 uvTransform;
};

// GPU用のParticleデータ
struct ParticleDataForGPU
{
  Matrix4x4 WVP;
  Matrix4x4 world;
  Vector4 color;
};

//Particle構造体
struct Particle
{
  Transform transform;
  Vector3 velocity;
  Vector4 color;
  float lifeTime;
  float currentTime;
};

// CS用のパーティクルデータ
struct ParticleCS
{
  Vector3 translate;
  Vector3 scale;
  Vector3 velocity;
  Vector4 color;
  float lifeTime;
  float currentTime;
};

struct PerView
{
  Matrix4x4 viewProjection;
  Matrix4x4 billboardMatrix;
};

struct PerFrame
{
  float time;
  float deltaTime;
};

// パーティクルグループ構造体
struct ParticleGroup
{
  // texture
  TextureData texture;
  // パーティクルのリスト
  std::list<Particle> particleList;
  // インスタンシングデータ用SRVインデックス
  int instancingSrvIndex;
  // GPU用のParticleデータリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> particleDataForGPUResource_;
  // インスタンシングデータを書き込むためのポインタ
  ParticleDataForGPU* pParticleDataForGPU = nullptr;
  // インスタンス数
  UINT instanceCount = 0;
};

// エミッター構造体
struct Emitter
{
  Transform transform;
  uint32_t count;
  float frequency;
  float frequencyTime;
};

struct EmitterSphere
{
  Vector3 center;
  float radius;
  uint32_t count;
  float frequency;
  float frequencyTime;
  uint32_t isEmit;
};