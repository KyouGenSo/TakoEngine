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
  constexpr uint32_t PFLAG_SCALE_FADE          = (1u << 3); ///< 寿命進行に応じてスケールを endScale へ補間
  constexpr uint32_t PFLAG_ALPHA_FADE          = (1u << 4); ///< 寿命進行に応じて alpha を 1.0 → 0.0 へ線形補間

  // ===== エミッター用ビットフラグ定数 =====
  constexpr uint32_t EFLAG_ACTIVE          = (1u << 0); ///< エミッターがアクティブ
  constexpr uint32_t EFLAG_EMITTING        = (1u << 1); ///< 現在射出中
  constexpr uint32_t EFLAG_NORMALIZE       = (1u << 2); ///< 速度ベクトルを正規化
  constexpr uint32_t EFLAG_RANDOM_ROTATE_Z = (1u << 3); ///< Z軸ランダム回転
  constexpr uint32_t EFLAG_USE_FORCE_FIELD = (1u << 4); ///< フォースフィールド有効
  constexpr uint32_t EFLAG_TEMPORARY       = (1u << 5); ///< 一時的なエミッター
  constexpr uint32_t EFLAG_USE_CURL_NOISE      = (1u << 6); ///< Curl Noise乱流有効
  constexpr uint32_t EFLAG_USE_DEPTH_COLLISION = (1u << 7); ///< 深度バッファ衝突有効
  constexpr uint32_t EFLAG_USE_SCALE_FADE      = (1u << 8); ///< スケール縮小消滅を有効化 (endScaleDefault に補間)
  constexpr uint32_t EFLAG_USE_ALPHA_FADE      = (1u << 9); ///< 寿命進行で alpha フェード (既定 ON、OFF で寿命中は不透明)
  constexpr uint32_t EFLAG_CONVERGE_TO_TARGET  = (1u << 10); ///< Per-Emitter Target 収束 (全粒子が targetPosition へバネ-ダンパ)
  constexpr uint32_t EFLAG_LOCK_TO_SPAWN       = (1u << 11); ///< Per-Particle Spawn 拘束 (粒子ごとに targetLocal へバネ-ダンパ)
  constexpr uint32_t EFLAG_BILLBOARD           = (1u << 12); ///< ビルボード(カメラ追従)。OFF で particle.rotate に従うワールド固定向き
  constexpr uint32_t EFLAG_RENDER_AS_MESH      = (1u << 13); ///< パーティクルを(quad でなく)選択メッシュ形状で描画。Mesh エミッターで明示 ON にして使う

  // ===== パラメータごとのランダム化フラグ (randomFlags 用) =====
  /// <remarks>
  /// randomFlags が 0 のときは旧来の「range != float2(0,0) ならランダム」自動判定が
  /// 後方互換として動作する。明示制御したい場合はビットを立てて使う。
  /// </remarks>
  constexpr uint32_t ERAND_SCALE_X  = (1u << 0); ///< X 方向スケールをランダム化
  constexpr uint32_t ERAND_SCALE_Y  = (1u << 1); ///< Y 方向スケールをランダム化
  constexpr uint32_t ERAND_VEL_X    = (1u << 2); ///< X 方向速度をランダム化
  constexpr uint32_t ERAND_VEL_Y    = (1u << 3); ///< Y 方向速度をランダム化
  constexpr uint32_t ERAND_VEL_Z    = (1u << 4); ///< Z 方向速度をランダム化
  constexpr uint32_t ERAND_LIFETIME = (1u << 5); ///< 寿命をランダム化

  /// <summary>
  /// エミッタータイプ列挙型
  /// </summary>
  enum class EmitterType : uint32_t {
    Sphere = 0,    ///< 球体エミッター
    Box = 1,       ///< 箱型エミッター
    Triangle = 2,  ///< 三角形エミッター
    Mesh = 3       ///< メッシュエミッター
  };

  /// <summary>
  /// パーティクル描画ブレンドモード (per-emitter)
  /// </summary>
  enum class ParticleBlendMode : uint32_t {
    Add    = 0,  ///< 加算         (SrcBlend=SRC_ALPHA, DestBlend=ONE)
    Screen = 1,  ///< スクリーン   (SrcBlend=INV_DEST_COLOR, DestBlend=ONE) — 既定 (旧挙動互換)
    Alpha  = 2   ///< アルファ合成 (SrcBlend=SRC_ALPHA, DestBlend=INV_SRC_ALPHA)
  };

  /// <summary>
  /// スポーン位置種別 (要望4: 中/外/線)
  /// </summary>
  /// <remarks>
  /// 形状ごとに対応の有無が異なる:
  ///  - Sphere: Inside / Surface のみ (Edge は頂点未定義のため Surface へフォールバック)
  ///  - Box: 3 種すべて対応 (Inside=範囲内、Surface=6面、Edge=12辺)
  ///  - Triangle: Surface / Edge のみ (Inside は 2D 形状で意味なし → Surface へフォールバック)
  ///  - Mesh: Surface=面、Edge=辺、Inside=SDF Rejection
  /// </remarks>
  enum class SpawnLocation : uint32_t {
    Inside  = 0,  ///< 中 (範囲内ランダム): 現状の挙動
    Surface = 1,  ///< 外 (境界面上)
    Edge    = 2   ///< 線 (頂点を繋ぐエッジ上)
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
    Vector3 scale;          ///< 開始時スケール（Emit 時に決定）
    Vector3 endScale;       ///< 終了時スケール（PFLAG_SCALE_FADE が立っているときのみ補間先として使用）
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
    uint32_t emitterId;         ///< 所属エミッター ID (Update.CS で EmitterData を逆引きするため Emit 時に書き込む)
    Vector3 targetLocal;        ///< スポーン時の座標 (Mesh ならメッシュローカル、それ以外は world)
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
    uint32_t frameCount;         ///< GPU 乱数 seed 用フレームカウンタ (HLSL PerFrame.frameCount と一致)
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
    uint32_t randomFlags;     ///< パラメータごとのランダム化フラグ（ERAND_* ビットフラグ、0 で旧来の自動判定）
    uint32_t spawnLocation;   ///< スポーン位置種別（SpawnLocation: 0=Inside, 1=Surface, 2=Edge）

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

    // --- スケール縮小消滅 ---
    Vector3 endScaleDefault;  ///< EFLAG_USE_SCALE_FADE 有効時の終端スケール（既定 (0,0,0) で完全消失）

    // --- Per-Emitter Target 収束 ---
    Vector3 targetPosition;     ///< 収束目標座標 (動的バインド時は毎フレーム CPU 側で同期)
    float convergeStiffness;    ///< バネ係数 k (大きいほど強く target へ引き寄せられる)
    float convergeDamping;      ///< ダンパ係数 d (大きいほど振動が抑えられる)

    // --- Mesh エミッタ ---
    uint32_t meshVertexSrvIndex; ///< Mesh 頂点 StructuredBuffer SRV インデックス (未使用なら 0)
    uint32_t meshIndexSrvIndex;  ///< Mesh インデックス StructuredBuffer SRV インデックス
    uint32_t meshTriangleCount;  ///< Mesh の三角形数 (indices.size() / 3)
    Matrix4x4 meshWorld;         ///< Mesh 自体の world 行列 (動的追従用、毎フレーム CPU 側で同期)
    Vector3 meshAabbMin;         ///< Mesh ローカル AABB 最小 (SDF UV 変換にも使用)
    Vector3 meshAabbMax;         ///< Mesh ローカル AABB 最大
    uint32_t meshAreaPrefixSumSrvIndex; ///< 0 で等確率フォールバック
    float meshTotalArea;
    uint32_t meshSkinnedVertexSrvIndex; ///< 0 で原頂点 SRV を使用

    // --- Per-Particle Spawn 拘束 ---
    float lockStiffness;         ///< 拘束バネ係数 k (粒子ごと targetLocal への引き寄せ力)
    float lockDamping;           ///< 拘束ダンパ係数 d

    // --- per-emitter 物理 / Curl Noise パラメーター ---
    float damping;              ///< 速度減衰係数（0.98-0.99 推奨）
    float collisionRestitution; ///< 反発係数（0-1）
    float particleRadius;       ///< パーティクルの衝突判定半径
    float noiseScale;           ///< Curl Noise の空間スケール
    float noiseStrength;        ///< Curl Noise の強度

    // --- 描画設定 (per-emitter) ---
    uint32_t blendMode;         ///< 描画ブレンドモード (ParticleBlendMode: 0=Add, 1=Screen, 2=Alpha)
    uint32_t textureSrvIndex;   ///< 使用テクスチャの SRV インデックス (0 で既定テクスチャ circle.dds にフォールバック)

    /// <summary>
    /// デフォルトコンストラクタ
    /// </summary>
    EmitterData() : type(static_cast<uint32_t>(EmitterType::Sphere)),
      flags(EFLAG_ACTIVE | EFLAG_USE_FORCE_FIELD | EFLAG_USE_ALPHA_FADE | EFLAG_BILLBOARD), // alpha フェード・ビルボードは既定 ON (旧挙動互換)
      emitterID(0), randomFlags(0),
      spawnLocation(static_cast<uint32_t>(SpawnLocation::Inside)),
      position({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
      scaleRangeX(), scaleRangeY(), velRangeX(), velRangeY(), velRangeZ(), lifeTimeRange(),
      startColorTint({ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f }),
      endColorTint({ .x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f }),
      count(20), frequency(0.5f), frequencyTime(0.0f),
      emitterLifeTime(0.0f), emitterCurrentTime(0.0f),
      radius(1.0f), boxSize(), boxRotation(),
      triangleV1(), triangleV2(), triangleV3(),
      endScaleDefault({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
      targetPosition({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
      convergeStiffness(8.0f), convergeDamping(2.0f),
      meshVertexSrvIndex(0), meshIndexSrvIndex(0), meshTriangleCount(0),
      meshWorld(), // 全 0 で初期化 → Mesh エミッタ生成時に identity で上書き
      meshAabbMin({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
      meshAabbMax({ .x = 0.0f, .y = 0.0f, .z = 0.0f }),
      meshAreaPrefixSumSrvIndex(0), meshTotalArea(0.0f),
      meshSkinnedVertexSrvIndex(0),
      lockStiffness(20.0f), lockDamping(3.0f),
      damping(0.99f), collisionRestitution(0.5f), particleRadius(0.5f),
      noiseScale(0.05f), noiseStrength(0.001f),
      blendMode(static_cast<uint32_t>(ParticleBlendMode::Screen)), textureSrvIndex(0)
    {}
  };

} // namespace Tako