static const int kMaxParticles = 1000000;

// エミッタータイプの定義
#define EMITTER_TYPE_SPHERE 0
#define EMITTER_TYPE_BOX 1
#define EMITTER_TYPE_TRIANGLE 2

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct Particle
{
    float3 translate;
    float3 scale;
    float3 rotate;
    float3 velocity;
    float4 startColor;
    float4 endColor;
    float lifeTime;
    float currentTime;
};

// エミッター共通構造体
struct Emitter
{
    // 基本情報
    uint type;                // エミッタータイプ
    uint isActive;            // アクティブ状態（1=有効、0=無効）
    uint isEmit;              // 射出フラグ（1=射出する、0=射出しない）
    uint isNormalize;         // 正規化フラグ
    uint isRandomRotateZ; // Z軸ランダム回転フラグ（1=ランダム、0=固定回転）
    uint emitterID;           // エミッターID
    
    float3 position;          // 中心/基準位置
    float2 scaleRangeX;       // パーティクルのXサイズ範囲（最小、最大）
    float2 scaleRangeY;       // パーティクルのYサイズ範囲（最小、最大）
    float2 velRangeX;         // パーティクルのX速度範囲（最小、最大）
    float2 velRangeY;         // パーティクルのY速度範囲（最小、最大）
    float2 velRangeZ;         // パーティクルのZ速度範囲（最小、最大）
    float2 lifeTimeRange;     // パーティクルの寿命範囲（最小、最大）
    float4 startColorTint;    // パーティクルの開始色（RGBA）
    float4 endColorTint;      // パーティクルの終了色（RGBA）
    
    uint  count;              // 1回の射出で生成するパーティクル数
    float frequency;          // 射出頻度（秒）
    float frequencyTime;      // 経過時間カウンター

    uint isTemp;              // 一時的なエミッターかどうか（1=一時的、0=永続）
    float emitterLifeTime;    // エミッターの寿命（秒）
    float emitterCurrentTime; // エミッターの経過時間

    // 球体用パラメータ
    float radius;             // 球体の半径
    
    // 箱型用パラメータ
    float3 boxSize;           // 箱の大きさ（幅、高さ、奥行き）
    float3 boxRotation;       // 箱の回転（X,Y,Z軸、度数法）
    
    // 三角形用パラメータ
    float3 triangleV1;        // 三角形の頂点1（相対座標）
    float3 triangleV2;        // 三角形の頂点2（相対座標）
    float3 triangleV3;        // 三角形の頂点3（相対座標）
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