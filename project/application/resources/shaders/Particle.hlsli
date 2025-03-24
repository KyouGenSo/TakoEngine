static const int kMaxParticles = 20480;

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
    float3 velocity;
    float4 color;
    float lifeTime;
    float currentTime;
};

// エミッター共通構造体
struct Emitter
{
    // 基本情報
    uint type;              // エミッタータイプ
    uint isActive;          // アクティブ状態（1=有効、0=無効）
    uint isEmit;            // 射出フラグ（1=射出する、0=射出しない）
    uint emitterID;         // エミッターID
    
    float3 position;        // 中心/基準位置
    float4 colorTint;       // パーティクルの色補正
    
    uint count;             // 1回の射出で生成するパーティクル数
    float frequency;        // 射出頻度（秒）
    float frequencyTime;    // 経過時間カウンター
    float pad;              // パディング（16byte境界用）

    // 球体用パラメータ
    float radius;           // 球体の半径
    float3 spherePad;       // パディング
    
    // 箱型用パラメータ
    float3 boxSize;         // 箱の大きさ（幅、高さ、奥行き）
    float boxPad1;          // パディング
    float3 boxRotation;     // 箱の回転（X,Y,Z軸、度数法）
    float boxPad2;          // パディング
    
    // 三角形用パラメータ
    float3 triangleV1;      // 三角形の頂点1（相対座標）
    float triPad1;          // パディング
    float3 triangleV2;      // 三角形の頂点2（相対座標）
    float triPad2;          // パディング
    float3 triangleV3;      // 三角形の頂点3（相対座標）
    float triPad3;          // パディング
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