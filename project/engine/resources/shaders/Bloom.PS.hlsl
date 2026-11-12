// 明るい所を抜き出し、半分ずつ縮小しながらぼかして、拡大しながら混ぜる。ExecuteBloom がエントリごとに 1 パスとして描く
#include "FullScreen.hlsli"
#include "Common.hlsli"

// 全パスで同じ並びにする（ExecuteBloom が C++ の BloomParam をそのまま渡す）
struct BloomParam
{
    float intensity;
    float threshold;
    float scatter;
    float padding;
    float3 tint;
    float padding2;
};

ConstantBuffer<BloomParam> gParam : register(b0);
Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gBloomTexture : register(t1);

float2 TexelSize()
{
    float2 size;
    gTexture.GetDimensions(size.x, size.y);
    return 1.0f / size;
}

// 13 タップの縮小（Call of Duty: Advanced Warfare の方式）。2x2 の箱を 5 つ重ねた重みで、小さく明るい点が動いてもちらつきにくい
float3 Downsample13(float2 uv)
{
    const float2 texel = TexelSize();

    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    const float3 a = gTexture.Sample(gLinearClamp, uv + texel * float2(-2.0f, -2.0f)).rgb;
    const float3 b = gTexture.Sample(gLinearClamp, uv + texel * float2(0.0f, -2.0f)).rgb;
    const float3 c = gTexture.Sample(gLinearClamp, uv + texel * float2(2.0f, -2.0f)).rgb;
    const float3 d = gTexture.Sample(gLinearClamp, uv + texel * float2(-2.0f, 0.0f)).rgb;
    const float3 e = gTexture.Sample(gLinearClamp, uv).rgb;
    const float3 f = gTexture.Sample(gLinearClamp, uv + texel * float2(2.0f, 0.0f)).rgb;
    const float3 g = gTexture.Sample(gLinearClamp, uv + texel * float2(-2.0f, 2.0f)).rgb;
    const float3 h = gTexture.Sample(gLinearClamp, uv + texel * float2(0.0f, 2.0f)).rgb;
    const float3 i = gTexture.Sample(gLinearClamp, uv + texel * float2(2.0f, 2.0f)).rgb;
    const float3 j = gTexture.Sample(gLinearClamp, uv + texel * float2(-1.0f, -1.0f)).rgb;
    const float3 k = gTexture.Sample(gLinearClamp, uv + texel * float2(1.0f, -1.0f)).rgb;
    const float3 l = gTexture.Sample(gLinearClamp, uv + texel * float2(-1.0f, 1.0f)).rgb;
    const float3 m = gTexture.Sample(gLinearClamp, uv + texel * float2(1.0f, 1.0f)).rgb;

    return (e + j + k + l + m) * 0.125f + (b + d + f + h) * 0.0625f + (a + c + g + i) * 0.03125f;
}

float4 Prefilter(VertexShaderOutput input) : SV_TARGET
{
    float3 color = Downsample13(input.texCoord);
    const float brightness = max(color.r, max(color.g, color.b));

    // しきい値の前後 knee の幅を 2 次曲線でつなぎ、境目で光が急に途切れないようにする（Unity URP と同じ式）
    const float knee = gParam.threshold * 0.5f;
    float soft = clamp(brightness - gParam.threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1e-4f);
    color *= max(brightness - gParam.threshold, soft) / max(brightness, 1e-4f);

    return float4(color, 1.0f);
}

float4 Downsample(VertexShaderOutput input) : SV_TARGET
{
    return float4(Downsample13(input.texCoord), 1.0f);
}

float4 Upsample(VertexShaderOutput input) : SV_TARGET
{
    const float2 texel = TexelSize();
    const float2 uv = input.texCoord;

    // 3x3 のテント（1-2-1 / 2-4-2 / 1-2-1）
    float3 sum = gTexture.Sample(gLinearClamp, uv).rgb * 4.0f;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(-1.0f, 0.0f)).rgb * 2.0f;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(1.0f, 0.0f)).rgb * 2.0f;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(0.0f, -1.0f)).rgb * 2.0f;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(0.0f, 1.0f)).rgb * 2.0f;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(-1.0f, -1.0f)).rgb;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(1.0f, -1.0f)).rgb;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(-1.0f, 1.0f)).rgb;
    sum += gTexture.Sample(gLinearClamp, uv + texel * float2(1.0f, 1.0f)).rgb;

    // α は描画先（1 段大きいミップ）と混ぜる割合。0 と 1 では 1 つの段しか残らないので両端を避ける
    return float4(sum / 16.0f, lerp(0.05f, 0.95f, gParam.scatter));
}

float4 Combine(VertexShaderOutput input) : SV_TARGET
{
    const float4 base = gTexture.Sample(gLinearClamp, input.texCoord);
    const float3 bloom = gBloomTexture.Sample(gLinearClamp, input.texCoord).rgb * gParam.tint * gParam.intensity;

    // 8bit の描画先で薄い光の階調が縞にならないよう、1 階調に満たないノイズで散らす（Interleaved Gradient Noise）。
    // 光の無い画素は入力のまま返すので、光が 1/16 階調に届くまではノイズも弱める
    const float noise = frac(52.9829189f * frac(dot(input.position.xy, float2(0.06711056f, 0.00583715f))));
    const float dither = (noise - 0.5f) / 255.0f * saturate(max(bloom.r, max(bloom.g, bloom.b)) * (255.0f * 16.0f));

    // α は入力のまま。描画物の外では α=0 で光だけ残り、重ねるときに加算になる
    return float4(base.rgb + bloom + dither, base.a);
}
