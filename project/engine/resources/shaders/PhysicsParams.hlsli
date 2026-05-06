#ifndef PHYSICS_PARAMS_HLSLI
#define PHYSICS_PARAMS_HLSLI

// 物理パラメータ定数バッファ構造体
struct PhysicsParams
{
    float  depthBias;            // 深度衝突のバイアス
    float3 pad0;                 // 16Bアライメント

    float3 gridOrigin;           // ハッシュグリッドの原点
    float  gridCellSize;         // セルサイズ

    uint3  gridDimensions;       // グリッド次元数 (64,64,64)
    uint   activeForceFieldCount;// アクティブなフォースフィールド数

    float4x4 invViewProj;        // 逆ビュープロジェクション行列（深度衝突用）

    float2 screenSize;           // スクリーンサイズ
    float  noiseTime;            // Curl Noise 用の時間オフセット（フレーム進行）
    float  pad1;                 // 16Bアライメント

    // 深度バッファ衝突用
    float4x4 viewProj;          // ビュープロジェクション行列（パーティクル→スクリーン投影用）
    float3   cameraPos;         // カメラ位置（法線方向決定用）
    float    pad2;              // 16Bアライメント
};

#endif // PHYSICS_PARAMS_HLSLI
