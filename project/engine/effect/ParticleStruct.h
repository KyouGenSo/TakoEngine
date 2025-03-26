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

// エミッタータイプの列挙型
enum class EmitterType : uint32_t {
  Sphere = 0,
  Box = 1,
  Triangle = 2
};

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

// Particle構造体
struct Particle
{
  Vector3 translate;    // 位置
  Vector3 scale;        // スケール
  Vector3 velocity;     // 速度
  Vector4 color;        // 色（アルファ値含む）
  float lifeTime;       // 寿命（秒）
  float currentTime;    // 経過時間
};

// CS用のパーティクルデータ
struct ParticleCS
{
  Vector3 translate;    // 位置
  Vector3 scale;        // スケール
  Vector3 velocity;     // 速度
  Vector4 color;        // 色（アルファ値含む）
  float lifeTime;       // 寿命（秒）
  float currentTime;    // 経過時間
};

// PerView構造体
struct PerView
{
  Matrix4x4 viewProjection;    // ビュープロジェクション行列
  Matrix4x4 billboardMatrix;   // ビルボード行列
};

// PerFrame構造体
struct PerFrame
{
  float time;                  // 時間
  float deltaTime;             // デルタタイム
  uint32_t activeEmitterCount; // アクティブなエミッター数
  uint32_t pad;                // パディング
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

// C++側エミッターデータ構造体
struct EmitterData {
  // 基本情報
  EmitterType type;         // エミッタータイプ
  bool isActive;            // アクティブ状態
  bool isEmitting;          // 現在射出中かどうか
  uint32_t emitterID;       // エミッターID

  Vector3 position;         // 中心/基準位置
  Vector2 scaleRangeX;      // Xスケール範囲
  Vector2 scaleRangeY;      // Yスケール範囌
  Vector2 velRangeX;        // X速度範囲
  Vector2 velRangeY;        // Y速度範囌
  Vector2 velRangeZ;        // Z速度範囌
  Vector2 lifeTimeRange;   // 寿命範囲
  Vector4 colorTint;        // 色補正

  uint32_t count;           // 1回の射出で生成するパーティクル数
  float frequency;          // 射出頻度（秒）
  float frequencyTime;      // 経過時間

  // 型固有のパラメータ
  union {
    struct { float radius; } sphere;                      // 球体用
    struct { Vector3 size; Vector3 rotation; } box;       // 箱型用
    struct { Vector3 v1; Vector3 v2; Vector3 v3; } triangle; // 三角形用
  };

  // デフォルトコンストラクタ
  EmitterData() : type(EmitterType::Sphere), isActive(true), isEmitting(false),
    emitterID(0), position({ 0.0f, 0.0f, 0.0f }), colorTint({ 1, 1, 1, 1 }),
    count(20), frequency(0.5f), frequencyTime(0.0f) {
    // 球体パラメータの初期化
    sphere.radius = 1.0f;
  }
};

// GPU側に送るエミッター構造体
struct EmitterGPUData
{
  // 基本情報
  uint32_t type;           // エミッタータイプ
  uint32_t isActive;       // アクティブ状態
  uint32_t isEmit;         // 射出フラグ
  uint32_t emitterID;      // エミッターID

  Vector3 position;        // 中心/基準位置
  Vector2 scaleRangeX;     // Xスケール範囲
  Vector2 scaleRangeY;     // Yスケール範囲
  Vector2 velRangeX;       // X速度範囲
  Vector2 velRangeY;       // Y速度範囲
  Vector2 velRangeZ;       // Z速度範囲
  Vector2 lifeTimeRange;   // 寿命範囲
  Vector4 colorTint;       // 色補正

  uint32_t count;          // パーティクル数
  float frequency;         // 射出頻度
  float frequencyTime;     // 経過時間

  // 球体用パラメータ
  float radius;            // 球体の半径

  // 箱型用パラメータ
  Vector3 boxSize;         // 箱の大きさ
  Vector3 boxRotation;     // 箱の回転

  // 三角形用パラメータ
  Vector3 triangleV1;      // 三角形の頂点1
  Vector3 triangleV2;      // 三角形の頂点2
  Vector3 triangleV3;      // 三角形の頂点3
};