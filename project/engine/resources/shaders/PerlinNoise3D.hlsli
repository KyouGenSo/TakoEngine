#ifndef PERLIN_NOISE_3D_HLSLI
#define PERLIN_NOISE_3D_HLSLI

/// ===================================================================
/// Improved Perlin Noise 3D (Ken Perlin 2002)
/// 3D Perlin Noise と Curl Noise の HLSL 実装
/// ===================================================================

// 標準 Perlin permutation テーブル（0-255 のシャッフル済み配列を 2 回繰り返し）
static const uint perm[512] = {
    151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
    140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
    247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
     57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
     74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
     60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
     65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
    200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
     52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
    207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
    119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
    129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
    218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
     81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
    184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
    222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180,
    // 繰り返し（2 回目）
    151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
    140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
    247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
     57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
     74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
     60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
     65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
    200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
     52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
    207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
    119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
    129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
    218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
     81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
    184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
    222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180
};

/// <summary>
/// 5 次補間関数（Improved Perlin Noise 用）
/// 1 次・2 次微分が 0, 1 で連続になるスムースステップ
/// </summary>
/// <param name="t">入力ベクトル（各成分 0-1）</param>
/// <returns>補間されたベクトル</returns>
float3 fade(float3 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

/// <summary>
/// グラディエント関数
/// ハッシュ値の下位 4 ビットから 12 方向のグラディエントを選択しドット積を計算
/// </summary>
/// <param name="hash">ハッシュ値</param>
/// <param name="p">位置ベクトル</param>
/// <returns>グラディエントとのドット積</returns>
float grad3d(int hash, float3 p)
{
    int h = hash & 15;
    float u = h < 8 ? p.x : p.y;
    float v = h < 4 ? p.y : (h == 12 || h == 14 ? p.x : p.z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

/// <summary>
/// 3D Perlin Noise
/// 格子点のグラディエントを 5 次補間で合成し、-1 から 1 の範囲のノイズ値を返す
/// </summary>
/// <param name="p">3D 座標</param>
/// <returns>ノイズ値（-1 ~ 1）</returns>
float perlinNoise3D(float3 p)
{
    // 格子セルの整数座標
    int3 i = int3(floor(p)) & 255;

    // セル内の小数部分
    float3 f = frac(p);

    // 5 次補間
    float3 u = fade(f);

    // ハッシュ値の計算（permutation テーブルを使用）
    int A  = perm[i.x] + i.y;
    int AA = perm[A] + i.z;
    int AB = perm[A + 1] + i.z;
    int B  = perm[i.x + 1] + i.y;
    int BA = perm[B] + i.z;
    int BB = perm[B + 1] + i.z;

    // 8 つの格子点からのグラディエント値をトリリニア補間
    float result = lerp(
        lerp(
            lerp(grad3d(perm[AA],     f),
                 grad3d(perm[BA],     f - float3(1, 0, 0)), u.x),
            lerp(grad3d(perm[AB],     f - float3(0, 1, 0)),
                 grad3d(perm[BB],     f - float3(1, 1, 0)), u.x),
            u.y),
        lerp(
            lerp(grad3d(perm[AA + 1], f - float3(0, 0, 1)),
                 grad3d(perm[BA + 1], f - float3(1, 0, 1)), u.x),
            lerp(grad3d(perm[AB + 1], f - float3(0, 1, 1)),
                 grad3d(perm[BB + 1], f - float3(1, 1, 1)), u.x),
            u.y),
        u.z);

    return result;
}

/// <summary>
/// Curl Noise 3D
/// 3 つの独立した Perlin Noise フィールドの curl（回転）を有限差分で近似
/// 発散ゼロのベクトル場を生成し、有機的な乱流を表現
/// </summary>
/// <param name="p">3D 座標</param>
/// <returns>Curl Noise ベクトル（発散ゼロ）</returns>
float3 curlNoise3D(float3 p)
{
    const float eps = 0.01;

    // 3 つの独立したノイズ関数（異なるオフセットで擬似的に独立化）
    float3 offsetX = float3(123.456, 0.0, 0.0);
    float3 offsetY = float3(0.0, 234.567, 0.0);
    float3 offsetZ = float3(0.0, 0.0, 345.678);

    // Nx の偏微分
    float dNx_dy = (perlinNoise3D(p + offsetX + float3(0, eps, 0)) - perlinNoise3D(p + offsetX - float3(0, eps, 0))) / (2.0 * eps);
    float dNx_dz = (perlinNoise3D(p + offsetX + float3(0, 0, eps)) - perlinNoise3D(p + offsetX - float3(0, 0, eps))) / (2.0 * eps);

    // Ny の偏微分
    float dNy_dx = (perlinNoise3D(p + offsetY + float3(eps, 0, 0)) - perlinNoise3D(p + offsetY - float3(eps, 0, 0))) / (2.0 * eps);
    float dNy_dz = (perlinNoise3D(p + offsetY + float3(0, 0, eps)) - perlinNoise3D(p + offsetY - float3(0, 0, eps))) / (2.0 * eps);

    // Nz の偏微分
    float dNz_dx = (perlinNoise3D(p + offsetZ + float3(eps, 0, 0)) - perlinNoise3D(p + offsetZ - float3(eps, 0, 0))) / (2.0 * eps);
    float dNz_dy = (perlinNoise3D(p + offsetZ + float3(0, eps, 0)) - perlinNoise3D(p + offsetZ - float3(0, eps, 0))) / (2.0 * eps);

    // curl = (dNz/dy - dNy/dz, dNx/dz - dNz/dx, dNy/dx - dNx/dy)
    float3 curl;
    curl.x = dNz_dy - dNy_dz;
    curl.y = dNx_dz - dNz_dx;
    curl.z = dNy_dx - dNx_dy;

    return curl;
}

#endif // PERLIN_NOISE_3D_HLSLI
