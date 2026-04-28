#ifndef PHYSICS_PARAMS_HLSLI
#define PHYSICS_PARAMS_HLSLI

// 物理パラメータ定数バッファ構造体
struct PhysicsParams
{
    float  damping;              // 速度減衰係数（0.98-0.99 推奨）
    float  collisionRestitution; // 反発係数（0-1）
    float  particleRadius;       // パーティクルの衝突判定半径
    float  depthBias;            // 深度衝突のバイアス

    float3 gridOrigin;           // ハッシュグリッドの原点
    float  gridCellSize;         // セルサイズ

    uint3  gridDimensions;       // グリッド次元数 (64,64,64)
    uint   activeForceFieldCount;// アクティブなフォースフィールド数

    float4x4 invViewProj;        // 逆ビュープロジェクション行列（深度衝突用）

    float2 screenSize;           // スクリーンサイズ
    float  noiseTime;            // Curl Noise 用の時間オフセット
    float  noiseScale;           // Curl Noise の空間スケール

    float  noiseStrength;        // Curl Noise の強度
    float3 pad;                  // パディング（16Bアライメント）

    // 深度バッファ衝突用
    float4x4 viewProj;          // ビュープロジェクション行列（パーティクル→スクリーン投影用）
    float3   cameraPos;         // カメラ位置（法線方向決定用）
    float    pad2;              // 16Bアライメント
};

#endif // PHYSICS_PARAMS_HLSLI
