#pragma once
#include <list>
#include <d3d12.h>
#include <wrl.h>

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "Matrix4x4.h"
#include "ModelStruct.h"

namespace Tako {

  // ===== パーティクル用ビットフラグ定数 =====
  /// パーティクルがフォースフィールドの影響を受けるか
  constexpr uint32_t PFLAG_USE_FORCE_FIELD = (1u << 0);
  /// パーティクルがCurl Noiseの影響を受けるか
  constexpr uint32_t PFLAG_USE_CURL_NOISE      = (1u << 1);
  constexpr uint32_t PFLAG_USE_DEPTH_COLLISION = (1u << 2); ///< 深度バッファ衝突有効

  // ===== エミッター用ビットフラグ定数 =====
  constexpr uint32_t EFLAG_ACTIVE          = (1u << 0); ///< エミッターがアクティブ
  constexpr uint32_t EFLAG_EMITTING        = (1u << 1); ///< 現在射出中
  constexpr uint32_t EFLAG_NORMALIZE       = (1u << 2); ///< 速度ベクトルを正規化
  constexpr uint32_t EFLAG_RANDOM_ROTATE_Z = (1u << 3); ///< Z軸ランダム回転
  constexpr uint32_t EFLAG_USE_FORCE_FIELD = (1u << 4); ///< フォースフィールド有効
  constexpr uint32_t EFLAG_TEMPORARY       = (1u << 5); ///< 一時的なエミッター
  constexpr uint32_t EFLAG_USE_CURL_NOISE      = (1u << 6); ///< Curl Noise乱流有効
  constexpr uint32_t EFLAG_USE_DEPTH_COLLISION = (1u << 7); ///< 深度バッファ衝突有効

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
    Matrix4x4 uvTransform; ///< UV 座標変換行列
  };

  /// <summary>
  /// GPU 用パーティクルデータ構造体
  /// インスタンシング描画用の各パーティクルのトランスフォームデータ
  /// </summary>
  struct ParticleDataForGPU
  {
    Matrix4x4 WVP;    ///< ワールドビュープロジェクション行列
    Matrix4x4 world;  ///< ワールド行列
    Vector4 color;    ///< パーティクルカラー（RGBA）
  };

  /// <summary>
  /// パーティクル構造体（CPU 側）
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
  /// Compute Shader 用パーティクルデータ構造体
  /// GPU 側でのパーティクル更新計算に使用
  /// HLSL 側の Particle 構造体とフィールド順序・サイズが完全一致する必要がある
  /// </summary>
  struct ParticleCS
  {
    Vector3 translate;      ///< 現在位置
    Vector3 prevPosition;   ///< 前フレーム位置（Verlet積分用）
    Vector3 scale;          ///< スケール
    Vector3 rotate;         ///< 回転（オイラー角）
    Vector3 velocity;       ///< 速度ベクトル
    Vector4 startColor;     ///< 開始時の色（アルファ値含む）
    Vector4 endColor;       ///< 終了時の色（アルファ値含む）
    float lifeTime;         ///< 寿命（秒）
    float currentTime;      ///< 生成からの経過時間（秒）
    float mass;             ///< 質量（衝突応答用）
    uint32_t cellIndex;     ///< 空間ハッシュ用セルインデックス
    uint32_t flags;         ///< パーティクルフラグ（PFLAG_* ビットフラグ）
    // --- per-emitter 物理 / Curl Noise パラメーター（spawn 時にエミッターからキャッシュ） ---
    float damping;              ///< 速度減衰係数
    float collisionRestitution; ///< 反発係数
    float particleRadius;       ///< 衝突判定半径
    float noiseScale;           ///< Curl Noise 空間スケール
    float noiseStrength;        ///< Curl Noise 強度
  };

  /// <summary>
  /// フォースフィールドタイプ列挙型
  /// </summary>
  enum class ForceFieldType : uint32_t {
    Gravity     = 0,  ///< 方向重力
    Directional = 1,  ///< 方向風（方向 + 減衰）
    Vortex      = 2,  ///< 渦（回転軸 + 強度 + 半径減衰）
    Attract     = 3,  ///< 吸引
    Repel       = 4,  ///< 反発
  };

  /// <summary>
  /// フォースフィールドデータ構造体（GPU 用）
  /// </summary>
  /// <remarks>
  /// affectMask は呼び出し側（ゲーム側）が独自に意味付けする汎用ビットフラグ。
  /// エンジン側は意味を解釈せず、CPU クエリ API のフィルタリングにのみ使用。
  /// GPU シェーダ側はこのフィールドを無視する（パーティクルは従来通り全力場の影響を受ける）。
  /// </remarks>
  struct ForceFieldData
  {
    uint32_t type;       ///< フォースタイプ（ForceFieldType）
    Vector3 position;    ///< フォースの中心位置
    Vector3 direction;   ///< 方向（Gravity, Directional で使用）
    float strength;      ///< 強度
    float radius;        ///< 影響半径（0 = 無限）
    float falloff;       ///< 減衰指数（distance^falloff）
    uint32_t affectMask; ///< 影響対象を識別する汎用ビットフラグ（既定 0xFFFFFFFF = 全部に作用）
    float pad;           ///< 16バイトアライメント用パディング
  };

  /// <summary>
  /// 物理パラメータ定数バッファ構造体（GPU 用）
  /// </summary>
  struct PhysicsParamsData
  {
    float depthBias;            ///< 深度衝突のバイアス
    float pad0[3];              ///< 16Bアライメント

    Vector3 gridOrigin;         ///< ハッシュグリッドの原点
    float gridCellSize;         ///< セルサイズ

    uint32_t gridDimX;          ///< グリッド次元数 X
    uint32_t gridDimY;          ///< グリッド次元数 Y
    uint32_t gridDimZ;          ///< グリッド次元数 Z
    uint32_t activeForceFieldCount; ///< アクティブなフォースフィールド数

    Matrix4x4 invViewProj;      ///< 逆ビュープロジェクション行列（深度衝突用）

    float screenWidth;          ///< スクリーン幅
    float screenHeight;         ///< スクリーン高さ
    float noiseTime;            ///< Curl Noise 用の時間オフセット（フレーム進行）
    float pad1;                 ///< 16Bアライメント

    // --- 深度バッファ衝突用 ---
    Matrix4x4 viewProj;         ///< ビュープロジェクション行列（パーティクル→スクリーン投影用）
    Vector3 cameraPos;          ///< カメラ位置（法線方向決定用）
    float pad2;                 ///< 16Bアライメント
  };

  /// <summary>
  /// ビュー共通データ構造体（GPU 用）
  /// フレーム内で共通のビュー関連データ
  /// </summary>
  struct PerView
  {
    Matrix4x4 viewProjection;    ///< ビュープロジェクション行列
    Matrix4x4 billboardMatrix;   ///< ビルボード行列（常にカメラを向く）
  };

  /// <summary>
  /// フレーム共通データ構造体（GPU 用）
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
    int instancingSrvIndex;                    ///< インスタンシングデータの SRV インデックス
    Microsoft::WRL::ComPtr<ID3D12Resource> particleDataForGPUResource_; ///< GPU 用インスタンシングデータリソース
    ParticleDataForGPU* pParticleDataForGPU = nullptr; ///< インスタンシングデータ書き込み先ポインタ
    UINT instanceCount = 0;                    ///< このフレームで描画するインスタンス数
  };

  /// <summary>
  /// 統合エミッターデータ構造体
  /// CPU側とGPU側で共通のデータ構造。HLSL側のEmitter構造体と同一のフィールド順序・サイズ
  /// StructuredBuffer経由でGPUに直接転送される
  /// </summary>
  struct EmitterData {
    uint32_t type;            ///< エミッタータイプ（EmitterType: 0=球体, 1=箱型, 2=三角形）
    uint32_t flags;           ///< エミッターフラグ（EFLAG_* ビットフラグ）
    uint32_t emitterID;       ///< エミッター固有の ID

    Vector3 position;         ///< エミッターの中心/基準位置
    Vector2 scaleRangeX;      ///< X スケールの範囲[min, max]
    Vector2 scaleRangeY;      ///< Y スケールの範囲[min, max]
    Vector2 velRangeX;        ///< X 方向速度の範囲[min, max]
    Vector2 velRangeY;        ///< Y 方向速度の範囲[min, max]
    Vector2 velRangeZ;        ///< Z 方向速度の範囲[min, max]
    Vector2 lifeTimeRange;    ///< パーティクル寿命の範囲[min, max]（秒）
    Vector4 startColorTint;   ///< 開始色の色調補正（RGBA）
    Vector4 endColorTint;     ///< 終了色の色調補正（RGBA）

    uint32_t count;           ///< 1回の射出で生成するパーティクル数
    float frequency;          ///< 射出頻度（秒）
    float frequencyTime;      ///< 射出タイマーの経過時間

    float emitterLifeTime;    ///< エミッターの寿命（一時エミッター用）
    float emitterCurrentTime; ///< エミッターの経過時間

    float radius;             ///< 球体エミッター用：半径
    Vector3 boxSize;          ///< 箱型エミッター用：サイズ
    Vector3 boxRotation;      ///< 箱型エミッター用：回転（オイラー角）
    Vector3 triangleV1;       ///< 三角形エミッター用：頂点1
    Vector3 triangleV2;       ///< 三角形エミッター用：頂点2
    Vector3 triangleV3;       ///< 三角形エミッター用：頂点3

    // --- per-emitter 物理 / Curl Noise パラメーター ---
    float damping;              ///< 速度減衰係数（0.98-0.99 推奨）
    float collisionRestitution; ///< 反発係数（0-1）
    float particleRadius;       ///< パーティクルの衝突判定半径
    float noiseScale;           ///< Curl Noise の空間スケール
    float noiseStrength;        ///< Curl Noise の強度

    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    EmitterData() : type(static_cast<uint32_t>(EmitterType::Sphere)),
      flags(EFLAG_ACTIVE | EFLAG_USE_FORCE_FIELD),
      emitterID(0), position({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
      scaleRangeX(), scaleRangeY(), velRangeX(), velRangeY(), velRangeZ(), lifeTimeRange(),
      startColorTint({ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f }),
      endColorTint({ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f }),
      count(20), frequency(0.5f), frequencyTime(0.0f),
      emitterLifeTime(0.0f), emitterCurrentTime(0.0f),
      radius(1.0f), boxSize(), boxRotation(),
      triangleV1(), triangleV2(), triangleV3(),
      damping(0.99f), collisionRestitution(0.5f), particleRadius(0.5f),
      noiseScale(0.05f), noiseStrength(0.001f)
    {}
  };

} // namespace Tako