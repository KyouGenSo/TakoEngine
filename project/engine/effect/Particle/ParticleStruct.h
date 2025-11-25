#pragma once
#include <list>
#include <d3d12.h>
#include <wrl.h>

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"
#include "ModelStruct.h"

/// <summary>
/// エミッタータイプ列挙型
/// </summary>
enum class EmitterType : uint32_t {
  Sphere = 0,    ///< 球体エミッター
  Box = 1,       ///< 箱型エミッター
  Triangle = 2   ///< 三角形エミッター
};

/// <summary>
/// パーティクルマテリアル構造体
/// </summary>
struct ParticleMaterial
{
  Vector4 color;         ///< マテリアルカラー（RGBA）
  Matrix4x4 uvTransform; ///< UV座標変換行列
};

/// <summary>
/// GPU用パーティクルデータ構造体
/// インスタンシング描画用の各パーティクルのトランスフォームデータ
/// </summary>
struct ParticleDataForGPU
{
  Matrix4x4 WVP;    ///< ワールドビュープロジェクション行列
  Matrix4x4 world;  ///< ワールド行列
  Vector4 color;    ///< パーティクルカラー（RGBA）
};

/// <summary>
/// パーティクル構造体（CPU側）
/// 個々のパーティクルの状態を保持
/// </summary>
struct Particle
{
  Vector3 translate;    ///< 位置
  Vector3 scale;        ///< スケール
  Vector3 velocity;     ///< 速度ベクトル
  Vector3 rotate;       ///< 回転（オイラー角）
  Vector4 startColor;   ///< 開始時の色（アルファ値含む）
  Vector4 endColor;     ///< 終了時の色（アルファ値含む）
  float lifeTime;       ///< 寿命（秒）
  float currentTime;    ///< 生成からの経過時間（秒）
};

/// <summary>
/// Compute Shader用パーティクルデータ構造体
/// GPU側でのパーティクル更新計算に使用
/// </summary>
struct ParticleCS
{
  Vector3 translate;    ///< 位置
  Vector3 scale;        ///< スケール
  Vector3 velocity;     ///< 速度ベクトル
  Vector3 rotate;       ///< 回転（オイラー角）
  Vector4 startColor;   ///< 開始時の色（アルファ値含む）
  Vector4 endColor;     ///< 終了時の色（アルファ値含む）
  float lifeTime;       ///< 寿命（秒）
  float currentTime;    ///< 生成からの経過時間（秒）
};

/// <summary>
/// ビュー共通データ構造体（GPU用）
/// フレーム内で共通のビュー関連データ
/// </summary>
struct PerView
{
  Matrix4x4 viewProjection;    ///< ビュープロジェクション行列
  Matrix4x4 billboardMatrix;   ///< ビルボード行列（常にカメラを向く）
};

/// <summary>
/// フレーム共通データ構造体（GPU用）
/// フレーム内で共通の時間・エミッター情報
/// </summary>
struct PerFrame
{
  float time;                  ///< ゲーム開始からの総時間（秒）
  float deltaTime;             ///< 前フレームからの経過時間（秒）
  uint32_t activeEmitterCount; ///< アクティブなエミッター数
  uint32_t pad;                ///< パディング（16バイトアライメント）
};

/// <summary>
/// パーティクルグループ構造体
/// 同じテクスチャを使用するパーティクルをグループ化して一括描画
/// </summary>
struct ParticleGroup
{
  TextureData texture;                       ///< 使用するテクスチャ情報
  std::list<Particle> particleList;          ///< このグループに属するパーティクルのリスト
  int instancingSrvIndex;                    ///< インスタンシングデータのSRVインデックス
  Microsoft::WRL::ComPtr<ID3D12Resource> particleDataForGPUResource_; ///< GPU用インスタンシングデータリソース
  ParticleDataForGPU* pParticleDataForGPU = nullptr; ///< インスタンシングデータ書き込み先ポインタ
  UINT instanceCount = 0;                    ///< このフレームで描画するインスタンス数
};

/// <summary>
/// C++側エミッターデータ構造体
/// エミッターの全設定パラメータを保持（CPU側）
/// </summary>
struct EmitterData {
  EmitterType type;         ///< エミッタータイプ（球体/箱型/三角形）
  bool isActive;            ///< エミッターがアクティブかどうか
  bool isEmitting;          ///< 現在射出中かどうか
  bool isNormalize;         ///< 速度ベクトルを正規化するか
  bool isRandomRotateZ;     ///< Z軸ランダム回転を有効にするか
  uint32_t emitterID;       ///< エミッター固有のID

  Vector3 position;         ///< エミッターの中心/基準位置
  Vector2 scaleRangeX;      ///< Xスケールの範囲[min, max]
  Vector2 scaleRangeY;      ///< Yスケールの範囲[min, max]
  Vector2 velRangeX;        ///< X方向速度の範囲[min, max]
  Vector2 velRangeY;        ///< Y方向速度の範囲[min, max]
  Vector2 velRangeZ;        ///< Z方向速度の範囲[min, max]
  Vector2 lifeTimeRange;    ///< パーティクル寿命の範囲[min, max]（秒）
  Vector4 startColorTint;   ///< 開始色の色調補正（RGBA）
  Vector4 endColorTint;     ///< 終了色の色調補正（RGBA）

  uint32_t count;           ///< 1回の射出で生成するパーティクル数
  float frequency;          ///< 射出頻度（秒）
  float frequencyTime;      ///< 射出タイマーの経過時間

  bool isTemp;              ///< 一時的なエミッターかどうか
  float emitterLifeTime;    ///< エミッターの寿命（一時エミッター用）
  float emitterCurrentTime; ///< エミッターの経過時間

  /// <summary>
  /// 型固有のパラメータ（共用体）
  /// </summary>
  union {
    struct { float radius; } sphere;                      ///< 球体用：半径
    struct { Vector3 size; Vector3 rotation; } box;       ///< 箱型用：サイズと回転
    struct { Vector3 v1; Vector3 v2; Vector3 v3; } triangle; ///< 三角形用：3頂点
  };

  // デフォルトコンストラクタ
  EmitterData() : type(EmitterType::Sphere), isActive(true), isEmitting(false),
    emitterID(0), position({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
    scaleRangeX(), scaleRangeY(), velRangeX(), velRangeY(), velRangeZ(), lifeTimeRange(),
    startColorTint({ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f }),
    endColorTint({ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f }),
    count(20), frequency(0.5f), frequencyTime(0.0f)
  {
    // 球体パラメータの初期化
    sphere.radius = 1.0f;
  }
};

/// <summary>
/// GPU側エミッター構造体
/// Compute ShaderでのパーティクルComputeに使用
/// 全エミッタータイプのパラメータを平坦化して保持
/// </summary>
struct EmitterGPUData
{
  uint32_t type;           ///< エミッタータイプ（0=球体, 1=箱型, 2=三角形）
  uint32_t isActive;       ///< アクティブ状態（0=無効, 1=有効）
  uint32_t isEmit;         ///< 射出フラグ（このフレームで射出するか）
  uint32_t isNormalize;    ///< 速度正規化フラグ
  uint32_t isRandomRotateZ; ///< Z軸ランダム回転フラグ
  uint32_t emitterID;      ///< エミッターID

  Vector3 position;        ///< エミッター中心/基準位置
  Vector2 scaleRangeX;     ///< Xスケールの範囲[min, max]
  Vector2 scaleRangeY;     ///< Yスケールの範囲[min, max]
  Vector2 velRangeX;       ///< X方向速度の範囲[min, max]
  Vector2 velRangeY;       ///< Y方向速度の範囲[min, max]
  Vector2 velRangeZ;       ///< Z方向速度の範囲[min, max]
  Vector2 lifeTimeRange;   ///< パーティクル寿命の範囲[min, max]（秒）
  Vector4 startColorTint;  ///< 開始色の色調補正（RGBA）
  Vector4 endColorTint;    ///< 終了色の色調補正（RGBA）

  uint32_t count;          ///< 1回の射出で生成するパーティクル数
  float frequency;         ///< 射出頻度（秒）
  float frequencyTime;     ///< 射出タイマーの経過時間

  uint32_t isTemp;          ///< 一時的なエミッターフラグ
  float emitterLifeTime;    ///< エミッターの寿命
  float emitterCurrentTime; ///< エミッターの経過時間

  float radius;            ///< 球体エミッター用：半径

  Vector3 boxSize;         ///< 箱型エミッター用：サイズ
  Vector3 boxRotation;     ///< 箱型エミッター用：回転（オイラー角）

  Vector3 triangleV1;      ///< 三角形エミッター用：頂点1
  Vector3 triangleV2;      ///< 三角形エミッター用：頂点2
  Vector3 triangleV3;      ///< 三角形エミッター用：頂点3
};