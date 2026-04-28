/// ============================================ ///
/// Decal.hlsli - Projective Decal 共通構造体
/// ============================================ ///

/// 頂点シェーダー出力
struct VertexShaderInput {
    float4 pos : POSITION;
};

struct VertexShaderOutput {
    float4 pos : SV_POSITION;
};

/// ビュー・プロジェクション情報（b0）
struct ViewData {
    float4x4 invViewProj;  // ビュー・プロジェクション逆行列
    float screenWidth;      // スクリーン幅
    float screenHeight;     // スクリーン高さ
    float2 padding;         // 16バイトアライメント用
};

/// 個別デカールデータ（b1）
struct DecalData {
    float4x4 decalWorldInverse; // ワールド→デカールローカル変換
    float4x4 decalWVP;         // キューブの World * View * Projection
    float4 color;               // デカールカラー（RGBA）
    int shapeType;              // 0:Circle, 1:Fan, 2:Rectangle
    float fanHalfAngle;         // 扇形の半角(rad)
    float edgeSoftness;         // エッジのぼかし量
    int useTexture;             // 0:プロシージャル, 1:テクスチャ使用
};
