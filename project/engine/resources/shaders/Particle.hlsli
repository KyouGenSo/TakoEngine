static const int kMaxParticles = 1000000;

// エミッタータイプの定義
#define EMITTER_TYPE_SPHERE 0
#define EMITTER_TYPE_BOX 1
#define EMITTER_TYPE_TRIANGLE 2
#define EMITTER_TYPE_MESH 3

// スポーン位置種別 (要望4: 中/外/線)
#define SPAWN_INSIDE  0  // 中: 範囲内ランダム
#define SPAWN_SURFACE 1  // 外: 境界面上
#define SPAWN_EDGE    2  // 線: 頂点を繋ぐエッジ上

// パーティクル用ビットフラグ定数
#define PFLAG_USE_FORCE_FIELD  (1u << 0)
#define PFLAG_USE_CURL_NOISE       (1u << 1)
#define PFLAG_USE_DEPTH_COLLISION  (1u << 2)
#define PFLAG_SCALE_FADE           (1u << 3) // 寿命進行で scale を endScale に補間
#define PFLAG_ALPHA_FADE           (1u << 4) // 寿命進行で alpha を 1.0 → 0.0 に補間

// エミッター用ビットフラグ定数
#define EFLAG_ACTIVE           (1u << 0)
#define EFLAG_EMITTING         (1u << 1)
#define EFLAG_NORMALIZE        (1u << 2)
#define EFLAG_RANDOM_ROTATE_Z  (1u << 3)
#define EFLAG_USE_FORCE_FIELD  (1u << 4)
#define EFLAG_TEMPORARY        (1u << 5)
#define EFLAG_USE_CURL_NOISE       (1u << 6)
#define EFLAG_USE_DEPTH_COLLISION  (1u << 7)
#define EFLAG_USE_SCALE_FADE       (1u << 8) // スケール縮小消滅
#define EFLAG_USE_ALPHA_FADE       (1u << 9) // alpha フェード (既定 ON、OFF で寿命中は不透明)
#define EFLAG_CONVERGE_TO_TARGET   (1u << 10) // Per-Emitter Target 収束
#define EFLAG_LOCK_TO_SPAWN        (1u << 11) // Per-Particle Spawn 拘束

// パラメータごとのランダム化フラグ（randomFlags 用）
// randomFlags == 0 のときは旧来の「range != float2(0,0) ならランダム」自動判定にフォールバック
#define ERAND_SCALE_X   (1u << 0)
#define ERAND_SCALE_Y   (1u << 1)
#define ERAND_VEL_X     (1u << 2)
#define ERAND_VEL_Y     (1u << 3)
#define ERAND_VEL_Z     (1u << 4)
#define ERAND_LIFETIME  (1u << 5)

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
    float3 scale;          // 開始時スケール
    float3 endScale;       // 終了時スケール（PFLAG_SCALE_FADE のときのみ補間先として使用）
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
    uint  emitterId;            // 所属エミッター ID (Update.CS で逆引き)
    float3 targetLocal;         // スポーン時の座標 (Mesh ならメッシュローカル、それ以外は world)
};

// エミッター共通構造体
struct Emitter
{
    uint   type;              // エミッタータイプ
    uint   flags;             // エミッターフラグ（EFLAG_* ビットフラグ）
    uint   emitterID;         // エミッターID
    uint   randomFlags;       // パラメータごとのランダム化フラグ（ERAND_*、0 で旧来自動判定）
    uint   spawnLocation;     // スポーン位置種別（SPAWN_INSIDE / SPAWN_SURFACE / SPAWN_EDGE）

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

    // --- スケール縮小消滅 ---
    float3 endScaleDefault;   // EFLAG_USE_SCALE_FADE 有効時の終端スケール

    // --- Per-Emitter Target 収束 ---
    float3 targetPosition;    // 収束目標座標 (CPU 側で毎フレーム同期)
    float  convergeStiffness; // バネ係数 k
    float  convergeDamping;   // ダンパ係数 d

    // --- Mesh エミッタ ---
    uint     meshVertexSrvIndex; // Mesh 頂点 SRV (シェーダ内 register bind は固定スロット使用)
    uint     meshIndexSrvIndex;
    uint     meshTriangleCount;
    float4x4 meshWorld;
    float3   meshAabbMin;
    float3   meshAabbMax;
    uint     meshAreaPrefixSumSrvIndex; // 0 で等確率フォールバック
    float    meshTotalArea;
    uint     meshSkinnedVertexSrvIndex; // 0 で原頂点 SRV を使用

    // --- Per-Particle Spawn 拘束 ---
    float    lockStiffness;
    float    lockDamping;

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
