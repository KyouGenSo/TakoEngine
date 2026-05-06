static const int kMaxParticles = 1000000;

// エミッタータイプの定義
#define EMITTER_TYPE_SPHERE 0
#define EMITTER_TYPE_BOX 1
#define EMITTER_TYPE_TRIANGLE 2

// パーティクル用ビットフラグ定数
#define PFLAG_USE_FORCE_FIELD  (1u << 0)
#define PFLAG_USE_CURL_NOISE       (1u << 1)
#define PFLAG_USE_DEPTH_COLLISION  (1u << 2)

// エミッター用ビットフラグ定数
#define EFLAG_ACTIVE           (1u << 0)
#define EFLAG_EMITTING         (1u << 1)
#define EFLAG_NORMALIZE        (1u << 2)
#define EFLAG_RANDOM_ROTATE_Z  (1u << 3)
#define EFLAG_USE_FORCE_FIELD  (1u << 4)
#define EFLAG_TEMPORARY        (1u << 5)
#define EFLAG_USE_CURL_NOISE       (1u << 6)
#define EFLAG_USE_DEPTH_COLLISION  (1u << 7)

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct Particle
{
    float3 translate;      // 現在位置
    float3 prevPosition;   // 前フレーム位置（Verlet積分用）
    float3 scale;          // スケール
    float3 rotate;         // 回転（オイラー角）
    float3 velocity;       // 速度ベクトル
    float4 startColor;     // 開始色（RGBA）
    float4 endColor;       // 終了色（RGBA）
    float lifeTime;        // 寿命（秒）
    float currentTime;     // 経過時間（秒）
    float mass;            // 質量（衝突応答用）
    uint  cellIndex;       // 空間ハッシュ用セルインデックス
    uint  flags;           // パーティクルフラグ（PFLAG_* ビットフラグ）
    // --- per-emitter からキャッシュされた物理 / Curl Noise パラメーター ---
    float damping;              // 速度減衰係数
    float collisionRestitution; // 反発係数
    float particleRadius;       // 衝突判定半径
    float noiseScale;           // Curl Noise 空間スケール
    float noiseStrength;        // Curl Noise 強度
};

// エミッター共通構造体
struct Emitter
{
    uint   type;              // エミッタータイプ
    uint   flags;             // エミッターフラグ（EFLAG_* ビットフラグ）
    uint   emitterID;         // エミッターID

    float3 position;          // 中心/基準位置
    float2 scaleRangeX;       // パーティクルXサイズ範囲（最小、最大）
    float2 scaleRangeY;       // パーティクルYサイズ範囲（最小、最大）
    float2 velRangeX;         // パーティクルX速度範囲（最小、最大）
    float2 velRangeY;         // パーティクルY速度範囲（最小、最大）
    float2 velRangeZ;         // パーティクルZ速度範囲（最小、最大）
    float2 lifeTimeRange;     // パーティクルの寿命範囲（最小、最大）
    float4 startColorTint;    // パーティクルの開始色（RGBA）
    float4 endColorTint;      // パーティクルの終了色（RGBA）

    uint   count;             // 1回の射出で生成するパーティクル数
    float  frequency;         // 射出頻度（秒）
    float  frequencyTime;     // 経過時間カウンター

    float  emitterLifeTime;   // エミッターの寿命（秒）
    float  emitterCurrentTime;// エミッターの経過時間

    // 球体用パラメータ
    float  radius;            // 球体の半径

    // 箱型用パラメータ
    float3 boxSize;           // 箱の大きさ（幅、高さ、奥行き）
    float3 boxRotation;       // 箱の回転（X,Y,Z軸、度数法）

    // 三角形用パラメータ
    float3 triangleV1;        // 三角形の頂点1（相対座標）
    float3 triangleV2;        // 三角形の頂点2（相対座標）
    float3 triangleV3;        // 三角形の頂点3（相対座標）

    // --- per-emitter 物理 / Curl Noise パラメーター ---
    float damping;              // 速度減衰係数
    float collisionRestitution; // 反発係数
    float particleRadius;       // 衝突判定半径
    float noiseScale;           // Curl Noise 空間スケール
    float noiseStrength;        // Curl Noise 強度
};

// パーフレーム情報構造体
struct PerFrame
{
    float time;                 // 時間
    float deltaTime;            // デルタタイム
    uint activeEmitterCount;    // アクティブなエミッター数
    uint pad;                   // パディング
};

// PerView情報構造体
struct PerView
{
    float4x4 viewProj;          // ビュープロジェクション行列
    float4x4 billboardMat;      // ビルボード行列
};
